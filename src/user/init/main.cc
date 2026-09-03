#include <sys/root_graph.hh>
#include <sys/types.hh>

extern "C" int main(sys::word_t argument0, sys::word_t) noexcept {
    if (argument0 == sys::root_graph::root_supervisor_role)
        sys::root_graph::supervision_thread_entry();
    return sys::root_graph::supervise();
}
