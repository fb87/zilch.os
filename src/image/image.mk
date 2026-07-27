# SPDX-License-Identifier: Apache-2.0
EARLYFS := $(OBJTREE)/image/earlyfs.tar
MANIFEST ?= $(SRCTREE)/src/image/manifests/minimal.toml
BOOTSTRAP_IMAGE_OBJ :=
KERNEL_DATA_OBJECTS :=

ifeq ($(ARCH),arm64)
BOOTSTRAP_IMAGE_OBJ := $(OBJTREE)/src/user/bootstrap/embedded_images.o
KERNEL_DATA_OBJECTS += $(BOOTSTRAP_IMAGE_OBJ)
endif

.PHONY: image
image: $(EARLYFS)

$(EARLYFS): $(USER_PROGRAM_ELFS) $(if $(filter 1,$(CONFIG_HYPERVISOR_SELFTEST)),$(GUEST_TEST_ELF),) $(MANIFEST)
	@mkdir -p $(dir $@)
	@printf '  EARLYFS %s\n' '$@'
	@$(SRCTREE)/tools/image/make_earlyfs.sh $(USER_OBJDIR)/init.elf $(USER_OBJDIR)/memory-server.elf $(USER_OBJDIR)/pager-client.elf $(USER_OBJDIR)/memory-client.elf $(MANIFEST) $@ $(if $(filter 1,$(CONFIG_HYPERVISOR_SELFTEST)),$(GUEST_TEST_ELF),-)

ifeq ($(ARCH),arm64)
$(BOOTSTRAP_IMAGE_OBJ): $(SRCTREE)/src/user/bootstrap/embedded_images.S $(USER_PROGRAM_ELFS)
	@mkdir -p $(dir $@)
	@printf '  UDATA   %s\n' '$@'
	@$(CC) $(KBUILD_CPPFLAGS) $(KBUILD_AFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@
endif
