#include <stdlib.h>
#include <string.h>

#include <sys/control.hh>
#include <sys/native.hh>
#include <sys/types.hh>

#include <abi/sys/v1/capability.hh>
#include <abi/sys/v1/control.hh>
#include <abi/sys/v1/memory.hh>

/*
 * A first-fit heap over pages the process maps for itself.
 *
 * There is no brk and no mmap: growing the heap means creating a frame
 * (charged to this task's page quota) and mapping it at the next address
 * above the heap base. That is why the heap is a bounded number of pages
 * rather than "whatever RAM is free" -- the quota, not the machine, is the
 * limit, and a task that exhausts it gets a null from malloc rather than
 * taking memory from anything else.
 *
 * Blocks carry a small header and live in one address-ordered list, so
 * adjacent free blocks can be coalesced on free. First fit rather than best
 * fit: the allocation patterns here are a shell and small utilities, where
 * the cost of scanning for a better fit buys nothing.
 */
namespace
{
    namespace abi = sys::abi::v1;

    inline constexpr sys::word_t page_size = 4096U;

    /*
     * Each heap page needs a capability slot to hold its frame. The window
     * starts clear of everything a process is born with (1-4, 10, 11, 14,
     * 15, 16) and clear of the selectors servers assign themselves (20-31),
     * and ends at the cspace's 256-slot capacity.
     */
    inline constexpr sys::capability_id_t heap_frame_base = 64U;
    inline constexpr sys::capability_id_t heap_frame_limit = 192U;
    inline constexpr sys::word_t heap_page_capacity = heap_frame_limit - heap_frame_base;

    struct block_header {
        size_t size;      // payload bytes, excluding this header
        block_header* next;
        bool free;
    };

    /* Headers are 8-byte aligned, and so is every payload, because the
     * header itself is a multiple of 8 and pages are page-aligned. */
    inline constexpr size_t header_size = sizeof(block_header);

    block_header* heap_head = nullptr;
    block_header* heap_tail = nullptr;
    sys::word_t heap_pages = 0U;

    [[nodiscard]] sys::word_t heap_end() noexcept {
        return sys::native::heap_address + heap_pages * page_size;
    }

    /*
     * Maps `pages` more pages at the end of the heap. Each needs its own
     * frame and its own capability slot; running out of either is an
     * ordinary allocation failure.
     */
    [[nodiscard]] bool grow_heap(sys::word_t pages) noexcept {
        const sys::word_t read_write = static_cast<sys::word_t>(abi::CapabilityRight::read) |
                                       static_cast<sys::word_t>(abi::CapabilityRight::write);
        const sys::word_t attrs = abi::encode_mapping_attributes(
            abi::memory_type::normal, abi::memory_shareability::inner_shareable);
        for (sys::word_t added = 0U; added < pages; ++added) {
            if (heap_pages >= heap_page_capacity)
                return false;
            const auto selector =
                static_cast<sys::capability_id_t>(heap_frame_base + heap_pages);
            if (!sys::native::ok(sys::control(abi::control_operation::frame_create, 0U, selector)))
                return false;
            if (!sys::native::ok(sys::control(abi::control_operation::map_frame,
                                              sys::native::own_space, selector, heap_end(),
                                              read_write, attrs))) {
                (void)sys::control(abi::control_operation::frame_destroy, selector);
                return false;
            }
            ++heap_pages;
        }
        return true;
    }

    [[nodiscard]] size_t align_up(size_t value) noexcept {
        return (value + 7U) & ~static_cast<size_t>(7U);
    }

    /*
     * Splits `block` if the remainder is worth tracking. A remainder too
     * small to hold a header plus a usable payload is left attached to the
     * allocation instead, which wastes a few bytes rather than creating a
     * block that can never satisfy anything.
     */
    void split(block_header* block, size_t size) noexcept {
        if (block->size < size + header_size + 16U)
            return;
        auto* remainder = reinterpret_cast<block_header*>(
            reinterpret_cast<unsigned char*>(block) + header_size + size);
        remainder->size = block->size - size - header_size;
        remainder->free = true;
        remainder->next = block->next;
        block->size = size;
        block->next = remainder;
        if (heap_tail == block)
            heap_tail = remainder;
    }
} // namespace

extern "C" {

void* malloc(size_t size) noexcept {
    if (size == 0U)
        return nullptr;
    const size_t wanted = align_up(size);

    for (block_header* block = heap_head; block != nullptr; block = block->next) {
        if (!block->free || block->size < wanted)
            continue;
        split(block, wanted);
        block->free = false;
        return reinterpret_cast<unsigned char*>(block) + header_size;
    }

    /* Nothing reusable: extend the heap by enough whole pages to hold the
     * header and payload. */
    const size_t needed = wanted + header_size;
    const sys::word_t pages = static_cast<sys::word_t>((needed + page_size - 1U) / page_size);
    const sys::word_t start = heap_end();
    if (!grow_heap(pages))
        return nullptr;

    auto* block = reinterpret_cast<block_header*>(static_cast<sys::uintptr_t>(start));
    block->size = pages * page_size - header_size;
    block->free = true;
    block->next = nullptr;
    if (heap_tail != nullptr)
        heap_tail->next = block;
    else
        heap_head = block;
    heap_tail = block;

    split(block, wanted);
    block->free = false;
    return reinterpret_cast<unsigned char*>(block) + header_size;
}

void free(void* pointer) noexcept {
    if (pointer == nullptr)
        return;
    auto* block = reinterpret_cast<block_header*>(static_cast<unsigned char*>(pointer) -
                                                  header_size);
    block->free = true;
    /*
     * Coalesce forward across every run of free neighbours. Only blocks that
     * are physically adjacent may be merged: the list is address-ordered but
     * a page boundary the heap never grew across would leave a gap, and
     * merging over one would hand out memory that is not mapped.
     */
    for (block_header* scan = heap_head; scan != nullptr; scan = scan->next) {
        while (scan->free && scan->next != nullptr && scan->next->free &&
               reinterpret_cast<unsigned char*>(scan) + header_size + scan->size ==
                   reinterpret_cast<unsigned char*>(scan->next)) {
            if (heap_tail == scan->next)
                heap_tail = scan;
            scan->size += header_size + scan->next->size;
            scan->next = scan->next->next;
        }
    }
}

void* calloc(size_t count, size_t size) noexcept {
    if (count != 0U && size > static_cast<size_t>(-1) / count)
        return nullptr; // the product would wrap
    const size_t total = count * size;
    void* memory = malloc(total);
    if (memory != nullptr)
        (void)memset(memory, 0, total);
    return memory;
}

void* realloc(void* pointer, size_t size) noexcept {
    if (pointer == nullptr)
        return malloc(size);
    if (size == 0U) {
        free(pointer);
        return nullptr;
    }
    auto* block = reinterpret_cast<block_header*>(static_cast<unsigned char*>(pointer) -
                                                  header_size);
    if (block->size >= size)
        return pointer;
    void* replacement = malloc(size);
    if (replacement == nullptr)
        return nullptr;
    (void)memcpy(replacement, pointer, block->size);
    free(pointer);
    return replacement;
}

} // extern "C"
