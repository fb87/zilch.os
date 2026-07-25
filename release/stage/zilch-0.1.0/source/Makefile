# SPDX-License-Identifier: Apache-2.0
SHELL 		:= /bin/sh
MAKEFLAGS 	+= --no-builtin-rules
.SUFFIXES:
.DEFAULT_GOAL := all

PROJECT 	?= zilch
VERSION 	?= 0.1.0
ARCH 		?= arm64
ifeq ($(origin PLATFORM),command line)
else
PLATFORM 	:= $(if $(filter arm64,$(ARCH)),qemu-arm64-virt,qemu-amd64-q35)
endif
O 		?= build/$(ARCH)/$(PLATFORM)

SRCTREE 	:= $(CURDIR)
OBJTREE 	:= $(abspath $(O))
LLVM 		?= 1
CROSS_COMPILE 	?=

SUPPORTED_ARCHES 		:= arm64 amd64
SUPPORTED_arm64_PLATFORMS 	:= qemu-arm64-virt
SUPPORTED_amd64_PLATFORMS 	:= qemu-amd64-q35
ifeq ($(filter $(ARCH),$(SUPPORTED_ARCHES)),)
$(error Unsupported ARCH=$(ARCH))
endif
ifeq ($(filter $(PLATFORM),$(SUPPORTED_$(ARCH)_PLATFORMS)),)
$(error PLATFORM=$(PLATFORM) is invalid for ARCH=$(ARCH))
endif

ifeq ($(LLVM),1)
CC 	:= clang
CXX 	:= clang++
LD 	:= ld.lld
NM 	:= $(shell command -v llvm-nm 2>/dev/null || command -v nm)
OBJCOPY := $(shell command -v llvm-objcopy 2>/dev/null || command -v objcopy)
OBJDUMP := $(shell command -v llvm-objdump 2>/dev/null || command -v objdump)
READELF := $(shell command -v llvm-readelf 2>/dev/null || command -v readelf)
else
CC 	:= $(CROSS_COMPILE)gcc
CXX 	:= $(CROSS_COMPILE)g++
LD 	:= $(CROSS_COMPILE)ld
NM 	:= $(CROSS_COMPILE)nm
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
READELF := $(CROSS_COMPILE)readelf
endif

KERNEL_ELF := $(OBJTREE)/$(PROJECT).elf
KERNEL_BIN := $(OBJTREE)/$(PROJECT).bin
KERNEL_MAP := $(OBJTREE)/$(PROJECT).map

ifeq ($(ARCH),arm64)
TARGET_FLAGS 	:= $(if $(filter 1,$(LLVM)),--target=aarch64-none-elf,)
ARCH_FLAGS 	:= -march=armv8-a -mgeneral-regs-only
LD_EMULATION 	:= -m aarch64elf
LDSCRIPT 	:= $(SRCTREE)/arch/arm64/kernel.ld
QEMU 		:= qemu-system-aarch64
QEMU_FLAGS 	:= -machine virt,gic-version=3,virtualization=on -cpu cortex-a57 -smp 2 -m 256M -nographic -no-reboot -kernel $(KERNEL_ELF)
else
TARGET_FLAGS 	:= $(if $(filter 1,$(LLVM)),--target=x86_64-none-elf,)
ARCH_FLAGS 	:= -m64 -mno-red-zone -mcmodel=kernel -mno-sse -mno-sse2 -mno-mmx
LD_EMULATION 	:= -m elf_x86_64
LDSCRIPT 	:= $(SRCTREE)/arch/amd64/kernel.ld
QEMU 		:= qemu-system-x86_64
QEMU_FLAGS 	:= -machine q35 -cpu max -smp 2 -m 256M -nographic -no-reboot -kernel $(KERNEL_ELF)
endif

INCLUDES 	:= -I$(SRCTREE)/arch/$(ARCH)/include -I$(SRCTREE)/platform/$(PLATFORM)/include -I$(SRCTREE)/include -I$(OBJTREE)/include/generated
WARNINGS 	:= -Wall -Wextra -Werror -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wundef -Wcast-align -Wcast-qual -Wformat=2 -Wimplicit-fallthrough
KBUILD_CPPFLAGS := $(TARGET_FLAGS) $(ARCH_FLAGS) $(INCLUDES) -D__ZILCH_ARCH_$(ARCH)=1 -D__ZILCH_PLATFORM_$(subst -,_,$(PLATFORM))=1
KBUILD_CFLAGS 	:= -std=c17 -ffreestanding -fno-builtin -fno-common -fdata-sections -ffunction-sections $(WARNINGS)
KBUILD_CXXFLAGS := -std=c++20 -ffreestanding -nostdinc++ -fno-builtin -fno-common -fno-exceptions -fno-rtti -fno-threadsafe-statics \
		   -fno-use-cxa-atexit -fno-unwind-tables -fno-asynchronous-unwind-tables -fdata-sections -ffunction-sections $(WARNINGS)
KBUILD_AFLAGS 	:= -ffreestanding
export SRCTREE OBJTREE ARCH PLATFORM CC CXX LD NM OBJCOPY OBJDUMP READELF
export TARGET_FLAGS ARCH_FLAGS LD_EMULATION
export KBUILD_CPPFLAGS KBUILD_CFLAGS KBUILD_CXXFLAGS KBUILD_AFLAGS

core-y 		:= arch/$(ARCH)/ platform/$(PLATFORM)/ kernel/ lib/
core-builtins 	:= $(patsubst %/,$(OBJTREE)/%/built-in.o,$(core-y))

CLANG_FORMAT 	?= clang-format

FORMAT_FILES 	:= $(shell find \
	arch platform kernel lib include user runtime \
	-type f \
	\( -name '*.cpp' -o \
	   -name '*.cc' -o \
	   -name '*.hpp' -o \
	   -name '*.h' \))

.PHONY: format
format:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { \
		echo "error: $(CLANG_FORMAT) not found"; exit 1; \
	}
	@$(CLANG_FORMAT) -i $(FORMAT_FILES)

.PHONY: format-check
format-check:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { \
		echo "error: $(CLANG_FORMAT) not found"; exit 1; \
	}
	@failed=0; \
	for file in $(FORMAT_FILES); do \
		$(CLANG_FORMAT) --dry-run --Werror "$$file" || failed=1; \
	done; \
	exit $$failed

USER_ELF := $(OBJTREE)/user/init.elf
EARLYFS := $(OBJTREE)/image/earlyfs.tar
MANIFEST ?= $(SRCTREE)/image/manifests/minimal.toml

include $(SRCTREE)/scripts/Makefile.user

.PHONY: all kernel image
all: kernel userspace image

kernel: $(KERNEL_ELF) $(KERNEL_BIN)

image: $(EARLYFS)

$(EARLYFS): $(USER_ELF) $(MANIFEST)
	@mkdir -p $(dir $@)
	@printf '  EARLYFS %s\n' '$@'
	@$(SRCTREE)/image/scripts/make-earlyfs.sh $(USER_ELF) $(MANIFEST) $@

$(OBJTREE)/%/built-in.o: FORCE
	@mkdir -p $(dir $@)
	@$(MAKE) -s --no-print-directory -f $(SRCTREE)/scripts/Makefile.build obj=$* __build

$(KERNEL_ELF): $(core-builtins) $(LDSCRIPT)
	@mkdir -p $(dir $@)
	@printf '  LD      %s\n' '$@'
	@$(LD) $(LD_EMULATION) -T $(LDSCRIPT) --gc-sections --build-id=none -Map=$(KERNEL_MAP) -o $@ $(core-builtins)
	@$(SRCTREE)/scripts/check-elf.sh $@

$(KERNEL_BIN): $(KERNEL_ELF)
	@printf '  OBJCOPY %s\n' '$@'
	@$(OBJCOPY) -O binary $< $@

.PHONY: arm64 amd64 run disasm clean help release release-clean
arm64:
	@$(MAKE) ARCH=arm64 PLATFORM=qemu-arm64-virt

amd64:
	@$(MAKE) ARCH=amd64 PLATFORM=qemu-amd64-q35

run: $(KERNEL_ELF)
	@$(SRCTREE)/scripts/run.sh $(KERNEL_ELF)

disasm: $(KERNEL_ELF)
	@$(OBJDUMP) -drS $< > $(OBJTREE)/$(PROJECT).asm

release: format-check
	@$(MAKE) ARCH=arm64 PLATFORM=qemu-arm64-virt all
	@$(MAKE) ARCH=amd64 PLATFORM=qemu-amd64-q35 all
	@$(SRCTREE)/scripts/make-release.sh $(SRCTREE) $(SRCTREE)/build $(PROJECT) $(VERSION)

release-clean:
	@rm -rf $(SRCTREE)/release

clean:
	@rm -rf $(O)
help:
	@echo 'make arm64 | make amd64'
	@echo 'make ARCH=arm64 PLATFORM=qemu-arm64-virt run'
	@echo 'make ARCH=amd64 PLATFORM=qemu-amd64-q35 run'
	@echo 'make release [VERSION=x.y.z]'
	@echo 'make userspace | make image'
	@echo 'make format | make format-check'
	@echo 'make release-clean'
.PHONY: FORCE
FORCE:
