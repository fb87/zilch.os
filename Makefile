# SPDX-License-Identifier: Apache-2.0
SHELL := /bin/sh
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:
.DEFAULT_GOAL := all

include mk/config.mk
include mk/toolchain.mk
include src/user/user.mk
include src/image/image.mk
ifeq ($(CONFIG_SELFTEST),1)
include tests/tests.mk
endif
include src/kernel/kernel.mk
include mk/checks.mk

.PHONY: all run clean arm64 amd64 debug certification release FORCE
all: userspace kernel image

arm64:
	@$(MAKE) ARCH=arm64 PLATFORM=qemu-arm64-virt all

amd64:
	@$(MAKE) ARCH=amd64 PLATFORM=qemu-amd64-q35 all

certification:
	@$(MAKE) BUILD_VARIANT=certification ARCH=arm64 PLATFORM=qemu-arm64-virt all

debug:
	@$(MAKE) BUILD_VARIANT=debug ARCH=arm64 PLATFORM=qemu-arm64-virt all

run: $(KERNEL_ELF)
	@CPUS=$(CPUS) MEMORY_MB=$(MEMORY_MB) $(SRCTREE)/tools/run/run.sh $(KERNEL_ELF)

clean:
	@rm -rf $(OBJTREE)

FORCE:
