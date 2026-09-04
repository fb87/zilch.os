# SPDX-License-Identifier: Apache-2.0
EARLYFS := $(OBJTREE)/image/earlyfs.img
BOOTSTRAP_IMAGE_OBJ :=
KERNEL_DATA_OBJECTS :=

ifeq ($(ARCH),arm64)
BOOTSTRAP_IMAGE_OBJ := $(OBJTREE)/src/user/bootstrap/embedded_images.o
KERNEL_DATA_OBJECTS += $(BOOTSTRAP_IMAGE_OBJ)
endif
ifeq ($(ARCH),amd64)
BOOTSTRAP_IMAGE_OBJ := $(OBJTREE)/src/user/bootstrap/embedded_images_amd64.o
KERNEL_DATA_OBJECTS += $(BOOTSTRAP_IMAGE_OBJ)
endif

DISK_IMAGE := $(OBJTREE)/image/disk.img
DISK_ROOTFS := $(SRCTREE)/tools/image/rootfs

.PHONY: image disk-image
image: $(EARLYFS)
disk-image: $(DISK_IMAGE)

# Rebuilds whenever anything under rootfs/ changes, found the same way
# EARLYFS's userspace-program dependency list is enumerated elsewhere in
# this tree -- a plain $(wildcard) over the fixture directory, since it is
# small and checked in rather than generated.
$(DISK_IMAGE): $(shell find $(DISK_ROOTFS) -type f 2>/dev/null)
	@mkdir -p $(dir $@)
	@printf '  DISKIMG %s\n' '$@'
	@$(SRCTREE)/tools/image/make_disk_image.sh $(DISK_ROOTFS) $@

$(EARLYFS): $(USER_PROGRAM_ELFS) $(if $(and $(filter arm64,$(ARCH)),$(filter 1,$(CONFIG_GUEST_TEST_ARM64))),$(GUEST_TEST_ELF),)
	@mkdir -p $(dir $@)
	@printf '  EARLYFS %s\n' '$@'
	@$(SRCTREE)/tools/image/make_earlyfs.py \
		--entry bin/init=$(USER_OBJDIR)/init.elf \
		--entry bin/memory-server=$(USER_OBJDIR)/memory-server.elf \
		--entry bin/control-plane=$(USER_OBJDIR)/control-plane.elf \
		$(if $(filter arm64,$(ARCH)),--entry bin/domain-manager=$(USER_OBJDIR)/domain-manager.elf) \
		--entry bin/console-server=$(USER_OBJDIR)/console-server.elf \
		--entry bin/serial-driver=$(USER_OBJDIR)/serial-driver.elf \
		--entry bin/argv-probe=$(USER_OBJDIR)/argv-probe.elf \
		--entry bin/exec-probe=$(USER_OBJDIR)/exec-probe.elf \
		--entry bin/fork-probe=$(USER_OBJDIR)/fork-probe.elf \
		--entry bin/libc-probe=$(USER_OBJDIR)/libc-probe.elf \
		--entry bin/vfs-server=$(USER_OBJDIR)/vfs-server.elf \
		--entry bin/vfs-probe=$(USER_OBJDIR)/vfs-probe.elf \
		$(if $(filter arm64,$(ARCH)),--entry bin/virtio-driver=$(USER_OBJDIR)/virtio-driver.elf) \
		--entry bin/pager-client=$(if $(filter 1,$(CONFIG_TESTS)),$(USER_OBJDIR)/pager-client.elf,-) \
		--entry bin/memory-client=$(if $(filter 1,$(CONFIG_TESTS)),$(USER_OBJDIR)/memory-client.elf,-) \
		$(if $(and $(filter arm64,$(ARCH)),$(filter 1,$(CONFIG_GUEST_TEST_ARM64))),--entry guests/test-arm64=$(GUEST_TEST_ELF)) \
		--output $@

ifeq ($(ARCH),arm64)
$(BOOTSTRAP_IMAGE_OBJ): $(SRCTREE)/src/user/bootstrap/embedded_images.S $(EARLYFS)
	@mkdir -p $(dir $@)
	@printf '  UDATA   %s\n' '$@'
	@$(CC) $(KBUILD_CPPFLAGS) $(KBUILD_AFLAGS) -DEARLYFS_IMAGE_PATH=\"$(EARLYFS)\" -MMD -MP -MF $(@:.o=.d) -c $< -o $@
endif
ifeq ($(ARCH),amd64)
$(BOOTSTRAP_IMAGE_OBJ): $(SRCTREE)/src/user/bootstrap/embedded_images_amd64.S $(EARLYFS)
	@mkdir -p $(dir $@)
	@printf '  UDATA   %s\n' '$@'
	@$(CC) $(KBUILD_CPPFLAGS) $(KBUILD_AFLAGS) -DEARLYFS_IMAGE_PATH=\"$(EARLYFS)\" -MMD -MP -MF $(@:.o=.d) -c $< -o $@
endif
