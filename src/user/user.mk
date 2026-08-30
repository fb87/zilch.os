# SPDX-License-Identifier: Apache-2.0
USER_OBJDIR := $(OBJTREE)/user
DOMAIN_GUEST_INTERACTIVE ?= 0
USER_LDSCRIPT := $(SRCTREE)/src/user/runtime/linker/user-$(ARCH).ld
USER_TEST_INCLUDES := $(if $(filter 1,$(CONFIG_SELFTEST)),-I$(SRCTREE)/tests/abi/include -I$(SRCTREE)/src/user/tests/include,)
USER_CPPFLAGS := $(TARGET_FLAGS) $(ARCH_FLAGS) $(USER_TEST_INCLUDES) \
    -I$(SRCTREE)/src/user/include -I$(SRCTREE)/include -I$(SRCTREE)/include/abi \
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
USER_PROGRAMS := init memory-server pager-client memory-client control-plane domain-manager
USER_init_SOURCE := src/user/init/main.cc
USER_memory-server_SOURCE := src/user/servers/memory/main.cc
USER_pager-client_SOURCE := src/user/tests/pager_client/main.cc
USER_memory-client_SOURCE := src/user/tests/memory_client/main.cc
USER_control-plane_SOURCE := src/user/servers/control_plane/main.cc
USER_domain-manager_SOURCE := src/user/servers/domain/main.cc
USER_ELF := $(USER_OBJDIR)/init.elf
USER_BIN := $(USER_OBJDIR)/init.bin
MEMORY_SERVER_BIN := $(USER_OBJDIR)/memory-server.bin
PAGER_CLIENT_BIN := $(USER_OBJDIR)/pager-client.bin
MEMORY_CLIENT_BIN := $(USER_OBJDIR)/memory-client.bin
CONTROL_PLANE_BIN := $(USER_OBJDIR)/control-plane.bin
DOMAIN_MANAGER_BIN := $(USER_OBJDIR)/domain-manager.bin
USER_PROGRAM_ELFS := $(addprefix $(USER_OBJDIR)/,$(addsuffix .elf,$(USER_PROGRAMS)))
USER_PROGRAM_BINS := $(USER_PROGRAM_ELFS:.elf=.bin)
USER_COMMON_OBJECTS := $(addprefix $(USER_OBJDIR)/common/,$(USER_COMMON_SOURCES:.cc=.o))
USER_CRT := $(USER_OBJDIR)/common/src/user/runtime/crt/$(ARCH)/start.o
USER_domain-manager_EXTRA_OBJECTS :=

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

$(USER_OBJDIR)/common/src/user/runtime/crt/$(ARCH)/%.o: $(SRCTREE)/src/user/runtime/crt/$(ARCH)/%.S
	@mkdir -p $(dir $@)
	@printf '  UAS     %s\n' '$@'
	@$(CC) $(USER_AFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

-include $(shell find $(USER_OBJDIR) -name '*.d' 2>/dev/null)

# Certification-only ARM64 guest executable. Guest code is a user-owned,
# independently linked target and must not include private kernel headers.
GUEST_TEST_DIR := $(USER_OBJDIR)/guests/test-arm64
GUEST_TEST_ELF := $(GUEST_TEST_DIR)/guest-test.elf
GUEST_TEST_BIN := $(GUEST_TEST_DIR)/guest-test.bin
GUEST_TEST_OBJ := $(GUEST_TEST_DIR)/entry.o
GUEST_TEST_LDSCRIPT := $(SRCTREE)/src/user/guests/test-arm64/linker.ld
DOMAIN_MANAGER_GUEST_BLOB_OBJ := $(USER_OBJDIR)/domain-manager/guest_blob.o
DOMAIN_GUEST_ELF ?= $(GUEST_TEST_ELF)

ifeq ($(ARCH),arm64)
ifeq ($(CONFIG_HYPERVISOR_SELFTEST),1)
userspace: $(GUEST_TEST_ELF) $(GUEST_TEST_BIN)
USER_domain-manager_EXTRA_OBJECTS += $(DOMAIN_MANAGER_GUEST_BLOB_OBJ)
$(USER_OBJDIR)/domain-manager.elf: $(DOMAIN_MANAGER_GUEST_BLOB_OBJ)
endif
endif

ifeq ($(ARCH),arm64)
ifeq ($(CONFIG_HYPERVISOR_SELFTEST),1)
$(DOMAIN_MANAGER_GUEST_BLOB_OBJ): $(SRCTREE)/src/user/servers/domain/guest_blob.S $(DOMAIN_GUEST_ELF)
	@mkdir -p $(dir $@)
	@printf '  GAS     %s\n' '$@'
	@$(CC) $(TARGET_FLAGS) -march=armv8-a -ffreestanding -DDOMAIN_GUEST_ELF_PATH=\"$(DOMAIN_GUEST_ELF)\" -MMD -MP -MF $(@:.o=.d) -c $< -o $@
endif
endif

$(GUEST_TEST_OBJ): $(SRCTREE)/src/user/guests/test-arm64/entry.S
	@mkdir -p $(dir $@)
	@printf '  GAS     %s\n' '$@'
	@$(CC) $(TARGET_FLAGS) -march=armv8-a -ffreestanding -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(GUEST_TEST_ELF): $(GUEST_TEST_OBJ) $(GUEST_TEST_LDSCRIPT)
	@mkdir -p $(dir $@)
	@printf '  GLD     %s\n' '$@'
	@$(LD) -m aarch64elf -T $(GUEST_TEST_LDSCRIPT) --gc-sections --build-id=none \
		-z max-page-size=0x1000 -Map=$(GUEST_TEST_DIR)/guest-test.map -o $@ $(GUEST_TEST_OBJ)

$(GUEST_TEST_BIN): $(GUEST_TEST_ELF)
	@printf '  GOBJCOPY %s\n' '$@'
	@$(OBJCOPY) -O binary $< $@

-include $(GUEST_TEST_OBJ:.o=.d)
