# SPDX-License-Identifier: Apache-2.0
USER_OBJDIR := $(OBJTREE)/user
DOMAIN_GUEST_INTERACTIVE ?= $(CONFIG_GUEST_INTERACTIVE)
USER_LDSCRIPT := $(SRCTREE)/src/user/runtime/linker/user-$(ARCH).ld
USER_TEST_INCLUDES := $(if $(filter 1,$(CONFIG_SELFTEST)),-I$(SRCTREE)/tests/abi/include -I$(SRCTREE)/src/user/tests/include,)
USER_CPPFLAGS := $(TARGET_FLAGS) $(ARCH_FLAGS) $(USER_TEST_INCLUDES) \
    -I$(OBJTREE)/include/generated -I$(SRCTREE)/src/user/include \
    -I$(SRCTREE)/src/user/personalities/native/include \
    -I$(SRCTREE)/src/user/domains/vmm/include \
    -I$(SRCTREE)/include -I$(SRCTREE)/include/abi \
    -include $(KCONFIG_AUTOCONF_H) \
    -DCONFIG_SELFTEST=$(CONFIG_SELFTEST) \
    -DCONFIG_HYPERVISOR_SELFTEST=$(CONFIG_HYPERVISOR_SELFTEST) \
    -DCONFIG_DOMAIN_GUEST_INTERACTIVE=$(DOMAIN_GUEST_INTERACTIVE) \
    -DCONFIG_VERBOSE_DIAGNOSTICS=$(CONFIG_VERBOSE_DIAGNOSTICS)
USER_CXXFLAGS := $(FREESTANDING_CXXFLAGS)
USER_AFLAGS := $(TARGET_FLAGS) $(ARCH_FLAGS) -ffreestanding
USER_COMMON_SOURCES := \
    src/user/lib/libsys/syscall.cc \
    src/user/lib/libruntime/runtime.cc \
    src/user/runtime/startup/process_entry.cc
USER_PROGRAMS := init memory-server control-plane console-server serial-driver
ifeq ($(ARCH),arm64)
USER_PROGRAMS += virtio-driver
endif
ifeq ($(ARCH),arm64)
USER_PROGRAMS += domain-manager
endif
ifeq ($(CONFIG_TESTS),1)
USER_PROGRAMS += pager-client memory-client
endif
USER_init_SOURCE := src/user/init/main.cc
USER_memory-server_SOURCE := src/user/servers/memory/main.cc
USER_pager-client_SOURCE := src/user/tests/pager_client/main.cc
USER_memory-client_SOURCE := src/user/tests/memory_client/main.cc
USER_control-plane_SOURCE := src/user/servers/control_plane/main.cc
USER_domain-manager_SOURCE := src/user/servers/domain/main.cc
USER_console-server_SOURCE := src/user/servers/console/main.cc
USER_serial-driver_SOURCE := src/user/drivers/serial/main.cc
USER_virtio-driver_SOURCE := src/user/drivers/virtio/main.cc
USER_ELF := $(USER_OBJDIR)/init.elf
USER_BIN := $(USER_OBJDIR)/init.bin
MEMORY_SERVER_BIN := $(USER_OBJDIR)/memory-server.bin
PAGER_CLIENT_BIN := $(USER_OBJDIR)/pager-client.bin
MEMORY_CLIENT_BIN := $(USER_OBJDIR)/memory-client.bin
CONTROL_PLANE_BIN := $(USER_OBJDIR)/control-plane.bin
DOMAIN_MANAGER_BIN := $(USER_OBJDIR)/domain-manager.bin
CONSOLE_SERVER_BIN := $(USER_OBJDIR)/console-server.bin
SERIAL_DRIVER_BIN := $(USER_OBJDIR)/serial-driver.bin
VIRTIO_DRIVER_BIN := $(USER_OBJDIR)/virtio-driver.bin
USER_PROGRAM_ELFS := $(addprefix $(USER_OBJDIR)/,$(addsuffix .elf,$(USER_PROGRAMS)))
USER_PROGRAM_BINS := $(USER_PROGRAM_ELFS:.elf=.bin)
USER_COMMON_OBJECTS := $(addprefix $(USER_OBJDIR)/common/,$(USER_COMMON_SOURCES:.cc=.o))
USER_CRT := $(USER_OBJDIR)/common/src/user/runtime/crt/$(ARCH)/start.o
USER_domain-manager_EXTRA_OBJECTS :=
USER_init_EXTRA_OBJECTS :=

.PHONY: userspace
userspace: $(USER_PROGRAM_ELFS) $(USER_PROGRAM_BINS)

$(USER_OBJDIR)/%.bin: $(USER_OBJDIR)/%.elf
	@printf '  UOBJCOPY %s\n' '$@'
	@$(OBJCOPY) -O binary $< $@

define build_user_program
USER_$(1)_OBJECT := $$(USER_OBJDIR)/$(1)/$$(USER_$(1)_SOURCE:.cc=.o)
$$(USER_OBJDIR)/$(1).elf: $$(USER_CRT) $$(USER_COMMON_OBJECTS) $$(USER_$(1)_OBJECT) $$(USER_$(1)_EXTRA_OBJECTS) $$(USER_LDSCRIPT)
	@mkdir -p $$(dir $$@)
	@printf '  ULD     %s\n' '$$@'
	@$$(LD) $$(LD_EMULATION) -T $$(USER_LDSCRIPT) --gc-sections --build-id=none \
		-z max-page-size=0x1000 -Map=$$(USER_OBJDIR)/$(1).map -o $$@ $$(USER_CRT) $$(USER_COMMON_OBJECTS) $$(USER_$(1)_OBJECT) $$(USER_$(1)_EXTRA_OBJECTS)
endef
$(foreach program,$(USER_PROGRAMS),$(eval $(call build_user_program,$(program))))

$(USER_OBJDIR)/common/%.o: $(SRCTREE)/%.cc
	@mkdir -p $(dir $@)
	@printf '  UCXX    %s\n' '$@'
	@$(CXX) $(USER_CPPFLAGS) $(USER_CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(USER_OBJDIR)/init/%.o: $(SRCTREE)/%.cc
	@mkdir -p $(dir $@)
	@printf '  UCXX    %s\n' '$@'
	@$(CXX) $(USER_CPPFLAGS) $(USER_CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@
$(USER_OBJDIR)/memory-server/%.o: $(SRCTREE)/%.cc
	@mkdir -p $(dir $@)
	@printf '  UCXX    %s\n' '$@'
	@$(CXX) $(USER_CPPFLAGS) $(USER_CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@
$(USER_OBJDIR)/pager-client/%.o: $(SRCTREE)/%.cc
	@mkdir -p $(dir $@)
	@printf '  UCXX    %s\n' '$@'
	@$(CXX) $(USER_CPPFLAGS) $(USER_CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@
$(USER_OBJDIR)/memory-client/%.o: $(SRCTREE)/%.cc
	@mkdir -p $(dir $@)
	@printf '  UCXX    %s\n' '$@'
	@$(CXX) $(USER_CPPFLAGS) $(USER_CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@
$(USER_OBJDIR)/control-plane/%.o: $(SRCTREE)/%.cc
	@mkdir -p $(dir $@)
	@printf '  UCXX    %s\n' '$@'
	@$(CXX) $(USER_CPPFLAGS) $(USER_CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@
$(USER_OBJDIR)/domain-manager/%.o: $(SRCTREE)/%.cc
	@mkdir -p $(dir $@)
	@printf '  UCXX    %s\n' '$@'
	@$(CXX) $(USER_CPPFLAGS) $(USER_CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@
$(USER_OBJDIR)/console-server/%.o: $(SRCTREE)/%.cc
	@mkdir -p $(dir $@)
	@printf '  UCXX    %s\n' '$@'
	@$(CXX) $(USER_CPPFLAGS) $(USER_CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@
$(USER_OBJDIR)/serial-driver/%.o: $(SRCTREE)/%.cc
	@mkdir -p $(dir $@)
	@printf '  UCXX    %s\n' '$@'
	@$(CXX) $(USER_CPPFLAGS) $(USER_CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@
$(USER_OBJDIR)/virtio-driver/%.o: $(SRCTREE)/%.cc
	@mkdir -p $(dir $@)
	@printf '  UCXX    %s\n' '$@'
	@$(CXX) $(USER_CPPFLAGS) $(USER_CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(USER_OBJDIR)/common/src/user/runtime/crt/$(ARCH)/%.o: $(SRCTREE)/src/user/runtime/crt/$(ARCH)/%.S
	@mkdir -p $(dir $@)
	@printf '  UAS     %s\n' '$@'
	@$(CC) $(USER_AFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

-include $(shell find $(USER_OBJDIR) -name '*.d' 2>/dev/null)

# Debug-only ARM64 verification guest. Guest code is a user-owned,
# independently linked target and must not include private kernel headers.
GUEST_TEST_DIR := $(USER_OBJDIR)/guests/test-arm64
GUEST_TEST_ELF := $(GUEST_TEST_DIR)/guest-test.elf
GUEST_TEST_BIN := $(GUEST_TEST_DIR)/guest-test.bin
GUEST_TEST_OBJ := $(GUEST_TEST_DIR)/entry.o
GUEST_TEST_LDSCRIPT := $(SRCTREE)/src/user/guests/test-arm64/linker.ld
DOMAIN_MANAGER_GUEST_BLOB_OBJ := $(USER_OBJDIR)/domain-manager/guest_blob.o
DOMAIN_GUEST_EARLYFS := $(USER_OBJDIR)/domain-manager/guest.img
DOMAIN_MANAGER_GUEST_MANIFEST_OBJ := $(USER_OBJDIR)/domain-manager/guest_manifest.o
INIT_GUEST_MANIFEST_OBJ := $(USER_OBJDIR)/init/guest_manifest.o
DOMAIN_GUEST_ELF ?= $(if $(filter 1,$(CONFIG_GUEST_TEST_ARM64)),$(GUEST_TEST_ELF),)
# A guest package may supply its own manifest describing the devices/IRQs/RAM
# it needs (see src/user/include/sys/guest_manifest.hh); this is otherwise a
# plain compiled translation unit, not an incbin blob, so any guest package
# just points this at its own .cc file matching that header's struct.
DOMAIN_GUEST_MANIFEST ?= $(SRCTREE)/src/user/servers/domain/default_manifest.cc

ifeq ($(ARCH),arm64)
ifeq ($(CONFIG_GUEST_TEST_ARM64),1)
userspace: $(GUEST_TEST_ELF) $(GUEST_TEST_BIN)
endif
ifeq ($(CONFIG_GUEST_EMBEDDED_IMAGE),1)
ifneq ($(strip $(DOMAIN_GUEST_ELF)),)
USER_domain-manager_EXTRA_OBJECTS += $(DOMAIN_MANAGER_GUEST_BLOB_OBJ)
$(USER_OBJDIR)/domain-manager.elf: $(DOMAIN_MANAGER_GUEST_BLOB_OBJ)
endif
endif
# The manifest is a plain compiled translation unit describing the devices/
# IRQs/RAM a guest needs -- independent of whether a guest *image* is
# embedded (that's DOMAIN_MANAGER_GUEST_BLOB_OBJ above, already separately
# conditioned on DOMAIN_GUEST_ELF). domain/main.cc and root_graph.hh
# reference sys_arm64_domain_guest_manifest unconditionally, so nesting this
# under CONFIG_GUEST_EMBEDDED_IMAGE left every guest-image-less arm64 build
# -- including the whole release profile, the only one that reaches
# root_graph.hh's supervise() -- failing to link on an undefined symbol.
USER_domain-manager_EXTRA_OBJECTS += $(DOMAIN_MANAGER_GUEST_MANIFEST_OBJ)
USER_init_EXTRA_OBJECTS += $(INIT_GUEST_MANIFEST_OBJ)
$(USER_OBJDIR)/domain-manager.elf: $(DOMAIN_MANAGER_GUEST_MANIFEST_OBJ)
$(USER_OBJDIR)/init.elf: $(INIT_GUEST_MANIFEST_OBJ)
endif

ifeq ($(ARCH),arm64)
ifeq ($(CONFIG_GUEST_EMBEDDED_IMAGE),1)
ifneq ($(strip $(DOMAIN_GUEST_ELF)),)
$(DOMAIN_GUEST_EARLYFS): $(DOMAIN_GUEST_ELF)
	@mkdir -p $(dir $@)
	@printf '  EARLYFS %s\n' '$@'
	@$(SRCTREE)/tools/image/make_earlyfs.py --entry guest.elf=$(DOMAIN_GUEST_ELF) --output $@
$(DOMAIN_MANAGER_GUEST_BLOB_OBJ): $(SRCTREE)/src/user/servers/domain/guest_blob.S $(DOMAIN_GUEST_EARLYFS)
	@mkdir -p $(dir $@)
	@printf '  GAS     %s\n' '$@'
	@$(CC) $(TARGET_FLAGS) -march=armv8-a -ffreestanding -DGUEST_EARLYFS_PATH=\"$(DOMAIN_GUEST_EARLYFS)\" -MMD -MP -MF $(@:.o=.d) -c $< -o $@
endif
endif
$(DOMAIN_MANAGER_GUEST_MANIFEST_OBJ) $(INIT_GUEST_MANIFEST_OBJ): $(DOMAIN_GUEST_MANIFEST)
	@mkdir -p $(dir $@)
	@printf '  UCXX    %s\n' '$@'
	@$(CXX) $(USER_CPPFLAGS) $(USER_CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@
endif

ifeq ($(ARCH),arm64)
$(GUEST_TEST_OBJ): $(SRCTREE)/src/user/guests/test-arm64/entry.S
	@mkdir -p $(dir $@)
	@printf '  GAS     %s\n' '$@'
	@$(CC) $(TARGET_FLAGS) -march=armv8-a -ffreestanding -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(GUEST_TEST_ELF): $(GUEST_TEST_OBJ) $(GUEST_TEST_LDSCRIPT)
	@mkdir -p $(dir $@)
	@printf '  GLD     %s\n' '$@'
	@$(LD) -m aarch64elf -T $(GUEST_TEST_LDSCRIPT) --gc-sections --build-id=none \
		-z max-page-size=0x1000 -Map=$(GUEST_TEST_DIR)/guest-test.map -o $@ $(GUEST_TEST_OBJ)
endif

$(GUEST_TEST_BIN): $(GUEST_TEST_ELF)
	@printf '  GOBJCOPY %s\n' '$@'
	@$(OBJCOPY) -O binary $< $@

-include $(GUEST_TEST_OBJ:.o=.d)
