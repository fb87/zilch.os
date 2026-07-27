# Production userspace image mapping fix

The ARM64 address-space bootstrap now maps every page of the embedded userspace image, rather than only its first 4 KiB page. User-context validation uses the same linked image extent. The userspace linker rejects images that exceed the current 64 KiB bootstrap window below `user_stack_base`.

Runtime verification remains pending through `make BUILD_VARIANT=certification run`. The expected first deliberate client fault is a data abort at `0x20004000`; an instruction abort within `0x20000000..0x2000ffff` indicates incomplete image population.
