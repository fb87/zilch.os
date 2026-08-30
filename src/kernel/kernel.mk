# SPDX-License-Identifier: Apache-2.0
KERNEL_ELF := $(OBJTREE)/$(PROJECT).elf
KERNEL_BIN := $(OBJTREE)/$(PROJECT).bin
KERNEL_MAP := $(OBJTREE)/$(PROJECT).map

KERNEL_PRIVATE_INCLUDES := \
    -I$(SRCTREE)/src/arch/$(ARCH)/include \
    -I$(SRCTREE)/src/platform/$(PLATFORM_DIR)/include \
    -I$(SRCTREE)/src/kernel/include
KERNEL_ABI_INCLUDES := -I$(SRCTREE)/include/abi -I$(SRCTREE)/include
KERNEL_TEST_INCLUDES := $(if $(filter 1,$(CONFIG_SELFTEST)),-I$(SRCTREE)/tests/include -I$(SRCTREE)/tests/abi/include,)
KBUILD_CPPFLAGS := $(TARGET_FLAGS) $(ARCH_FLAGS) $(KERNEL_TEST_INCLUDES) $(KERNEL_PRIVATE_INCLUDES) $(KERNEL_ABI_INCLUDES) \
    -I$(OBJTREE)/include/generated \
    -DCONFIG_ROOT_ONLY_BOOT=$(if $(filter root,$(BOOT_PROFILE)),1,0) \
    -DCONFIG_SELFTEST=$(CONFIG_SELFTEST) \
    -DCONFIG_HYPERVISOR_SELFTEST=$(CONFIG_HYPERVISOR_SELFTEST) \
    -DCONFIG_VERBOSE_DIAGNOSTICS=$(CONFIG_VERBOSE_DIAGNOSTICS) \
    -DCONFIG_TRACE=$(CONFIG_TRACE) \
    -DCONFIG_QEMU_RAM_MB=$(MEMORY_MB) -DCONFIG_QEMU_CPUS=$(CPUS) \
    -DUSER_ELF_PATH=\"$(USER_OBJDIR)/init.elf\" \
    -DMEMORY_SERVER_ELF_PATH=\"$(USER_OBJDIR)/memory-server.elf\" \
    -DCONTROL_PLANE_ELF_PATH=\"$(USER_OBJDIR)/control-plane.elf\" \
    -DDOMAIN_MANAGER_ELF_PATH=\"$(USER_OBJDIR)/domain-manager.elf\" \
    -DPAGER_CLIENT_ELF_PATH=\"$(USER_OBJDIR)/pager-client.elf\" \
    -DMEMORY_CLIENT_ELF_PATH=\"$(USER_OBJDIR)/memory-client.elf\" \
    -DGUEST_TEST_BIN_PATH=\"$(GUEST_TEST_BIN)\"
KBUILD_CXXFLAGS := $(FREESTANDING_CXXFLAGS)
KBUILD_AFLAGS := -ffreestanding
export KBUILD_CPPFLAGS KBUILD_CXXFLAGS KBUILD_AFLAGS

kernel-core-y := src/arch/$(ARCH)/ src/platform/$(PLATFORM_DIR)/ src/kernel/
kernel-core-builtins := $(patsubst %/,$(OBJTREE)/%/built-in.o,$(kernel-core-y))

.PHONY: kernel
kernel: userspace $(KERNEL_ELF) $(KERNEL_BIN)

$(OBJTREE)/%/built-in.o: FORCE
	@mkdir -p $(dir $@)
	@$(MAKE) -s --no-print-directory -f $(SRCTREE)/tools/build/Makefile.build obj=$* __build

$(KERNEL_ELF): $(USER_BIN) $(kernel-core-builtins) $(KERNEL_DATA_OBJECTS) $(KERNEL_LDSCRIPT)
	@mkdir -p $(dir $@)
	@printf '  LD      %s\n' '$@'
	@$(LD) $(LD_EMULATION) -T $(KERNEL_LDSCRIPT) --gc-sections --build-id=none -Map=$(KERNEL_MAP) -o $@ $(kernel-core-builtins) $(KERNEL_DATA_OBJECTS)
	@$(SRCTREE)/tools/build/check_elf.sh $@

$(KERNEL_BIN): $(KERNEL_ELF)
	@printf '  OBJCOPY %s\n' '$@'
	@$(OBJCOPY) -O binary $< $@
