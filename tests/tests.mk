# SPDX-License-Identifier: Apache-2.0
CERTIFICATION_GUEST_BLOB_OBJ :=
ifeq ($(CONFIG_HYPERVISOR_SELFTEST),1)
ifeq ($(ARCH),arm64)
CERTIFICATION_GUEST_BLOB_OBJ := $(OBJTREE)/tests/hypervisor/fixtures/guest_blob.o
KERNEL_DATA_OBJECTS += $(CERTIFICATION_GUEST_BLOB_OBJ)

$(CERTIFICATION_GUEST_BLOB_OBJ): $(SRCTREE)/tests/hypervisor/fixtures/guest_blob.S $(GUEST_TEST_BIN)
	@mkdir -p $(dir $@)
	@printf '  TDATA   %s\n' '$@'
	@$(CC) $(KBUILD_CPPFLAGS) $(KBUILD_AFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@
endif
endif
