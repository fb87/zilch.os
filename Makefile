# SPDX-License-Identifier: Apache-2.0
SHELL := /bin/sh
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:
.DEFAULT_GOAL := all

PROJECT ?= zilch
VERSION ?= 0.7.0
BOOT_PROFILE ?= root
BUILD_VARIANT ?= development
ARCH ?= arm64

VALID_BUILD_VARIANTS := development certification release
ifeq ($(filter $(BUILD_VARIANT),$(VALID_BUILD_VARIANTS)),)
$(error BUILD_VARIANT=$(BUILD_VARIANT) is invalid; expected one of $(VALID_BUILD_VARIANTS))
endif
CONFIG_SELFTEST := $(if $(filter certification,$(BUILD_VARIANT)),1,0)
CONFIG_HYPERVISOR_SELFTEST := $(CONFIG_SELFTEST)
CONFIG_VERBOSE_DIAGNOSTICS := $(if $(filter development certification,$(BUILD_VARIANT)),1,0)
ifeq ($(origin PLATFORM),command line)
else
PLATFORM := $(if $(filter arm64,$(ARCH)),qemu-arm64-virt,qemu-amd64-q35)
endif
PLATFORM_DIR_qemu-arm64-virt := qemu_arm64_virt
PLATFORM_DIR_qemu-amd64-q35 := qemu_amd64_q35
PLATFORM_DIR := $(PLATFORM_DIR_$(PLATFORM))
O ?= out/build/$(ARCH)/$(PLATFORM)
SRCTREE := $(CURDIR)
OBJTREE := $(abspath $(O))
LLVM ?= 1
CROSS_COMPILE ?=

SUPPORTED_ARCHES := arm64 amd64
SUPPORTED_arm64_PLATFORMS := qemu-arm64-virt
SUPPORTED_amd64_PLATFORMS := qemu-amd64-q35
ifeq ($(filter $(ARCH),$(SUPPORTED_ARCHES)),)
$(error Unsupported ARCH=$(ARCH))
endif
ifeq ($(filter $(PLATFORM),$(SUPPORTED_$(ARCH)_PLATFORMS)),)
$(error PLATFORM=$(PLATFORM) is invalid for ARCH=$(ARCH))
endif

ifeq ($(LLVM),1)
CC := clang
CXX := clang++
LD := ld.lld
NM := $(shell command -v llvm-nm 2>/dev/null || command -v nm)
OBJCOPY := $(shell command -v llvm-objcopy 2>/dev/null || command -v objcopy)
OBJDUMP := $(shell command -v llvm-objdump 2>/dev/null || command -v objdump)
READELF := $(shell command -v llvm-readelf 2>/dev/null || command -v readelf)
else
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
LD := $(CROSS_COMPILE)ld
NM := $(CROSS_COMPILE)nm
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
READELF := $(CROSS_COMPILE)readelf
endif

KERNEL_ELF := $(OBJTREE)/$(PROJECT).elf
KERNEL_BIN := $(OBJTREE)/$(PROJECT).bin
KERNEL_MAP := $(OBJTREE)/$(PROJECT).map
ifeq ($(ARCH),arm64)
TARGET_FLAGS := $(if $(filter 1,$(LLVM)),--target=aarch64-none-elf,)
ARCH_FLAGS := -march=armv8-a -mgeneral-regs-only
LD_EMULATION := -m aarch64elf
LDSCRIPT := $(SRCTREE)/src/arch/arm64/kernel.ld
else
TARGET_FLAGS := $(if $(filter 1,$(LLVM)),--target=x86_64-none-elf,)
ARCH_FLAGS := -m64 -mno-red-zone -mcmodel=kernel -mno-sse -mno-sse2 -mno-mmx
LD_EMULATION := -m elf_x86_64
LDSCRIPT := $(SRCTREE)/src/arch/amd64/kernel.ld
endif

INCLUDES := -I$(SRCTREE)/src/arch/$(ARCH)/include \
            -I$(SRCTREE)/src/platform/$(PLATFORM_DIR)/include \
            -I$(SRCTREE)/src/kernel/include \
            -I$(SRCTREE)/include \
            -I$(SRCTREE)/include/abi \
            -I$(OBJTREE)/include/generated
WARNINGS := -Wall -Wextra -Werror -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wundef -Wcast-align -Wcast-qual -Wformat=2 -Wimplicit-fallthrough
KBUILD_CPPFLAGS := $(TARGET_FLAGS) $(ARCH_FLAGS) $(INCLUDES) -DCONFIG_ROOT_ONLY_BOOT=$(if $(filter root,$(BOOT_PROFILE)),1,0) \
    -DCONFIG_SELFTEST=$(CONFIG_SELFTEST) \
    -DCONFIG_HYPERVISOR_SELFTEST=$(CONFIG_HYPERVISOR_SELFTEST) \
    -DCONFIG_VERBOSE_DIAGNOSTICS=$(CONFIG_VERBOSE_DIAGNOSTICS) \
    -DUSER_BIN_PATH=\"$(OBJTREE)/user/init.bin\"
KBUILD_CXXFLAGS := -std=c++20 -ffreestanding -nostdinc++ -fno-builtin -fno-common -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit -fno-unwind-tables -fno-asynchronous-unwind-tables -fdata-sections -ffunction-sections $(WARNINGS)
KBUILD_AFLAGS := -ffreestanding
export SRCTREE OBJTREE ARCH PLATFORM BUILD_VARIANT CONFIG_SELFTEST CONFIG_HYPERVISOR_SELFTEST CONFIG_VERBOSE_DIAGNOSTICS CC CXX LD NM OBJCOPY OBJDUMP READELF TARGET_FLAGS ARCH_FLAGS LD_EMULATION KBUILD_CPPFLAGS KBUILD_CXXFLAGS KBUILD_AFLAGS

core-y := src/arch/$(ARCH)/ src/platform/$(PLATFORM_DIR)/ src/kernel/
core-builtins := $(patsubst %/,$(OBJTREE)/%/built-in.o,$(core-y))

CLANG_FORMAT ?= clang-format
FORMAT_FILES := $(shell find include src tools -type f \( -name '*.cc' -o -name '*.hh' -o -name '*.tt' \))
.PHONY: format format-check
format:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo 'error: $(CLANG_FORMAT) not found'; exit 1; }
	@$(CLANG_FORMAT) -i $(filter %.cc %.hh,$(FORMAT_FILES))
	@for file in $(filter %.tt,$(FORMAT_FILES)); do $(CLANG_FORMAT) -i --assume-filename="$${file%.tt}.cc" "$$file"; done
format-check:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo 'error: $(CLANG_FORMAT) not found'; exit 1; }
	@failed=0; for file in $(FORMAT_FILES); do case "$$file" in *.tt) extra="--assume-filename=$${file%.tt}.cc";; *) extra="";; esac; $(CLANG_FORMAT) $$extra --dry-run --Werror "$$file" || failed=1; done; exit $$failed

USER_ELF := $(OBJTREE)/user/init.elf
USER_BIN := $(OBJTREE)/user/init.bin
EARLYFS := $(OBJTREE)/image/earlyfs.tar
MANIFEST ?= $(SRCTREE)/src/image/manifests/minimal.toml
include $(SRCTREE)/tools/build/Makefile.user

.PHONY: all kernel image userspace arm64 amd64 run clean release certification production-gate
all: userspace kernel image
kernel: userspace $(KERNEL_ELF) $(KERNEL_BIN)
image: $(EARLYFS)
$(EARLYFS): $(USER_ELF) $(MANIFEST)
	@mkdir -p $(dir $@)
	@printf '  EARLYFS %s\n' '$@'
	@$(SRCTREE)/tools/image/make_earlyfs.sh $(USER_ELF) $(MANIFEST) $@
$(OBJTREE)/%/built-in.o: FORCE
	@mkdir -p $(dir $@)
	@$(MAKE) -s --no-print-directory -f $(SRCTREE)/tools/build/Makefile.build obj=$* __build
ifeq ($(ARCH),arm64)
$(OBJTREE)/src/arch/arm64/built-in.o: $(USER_BIN)
endif
$(KERNEL_ELF): $(USER_BIN) $(core-builtins) $(LDSCRIPT)
	@mkdir -p $(dir $@)
	@printf '  LD      %s\n' '$@'
	@$(LD) $(LD_EMULATION) -T $(LDSCRIPT) --gc-sections --build-id=none -Map=$(KERNEL_MAP) -o $@ $(core-builtins)
	@$(SRCTREE)/tools/build/check_elf.sh $@
$(KERNEL_BIN): $(KERNEL_ELF)
	@printf '  OBJCOPY %s\n' '$@'
	@$(OBJCOPY) -O binary $< $@
arm64:
	@$(MAKE) ARCH=arm64 PLATFORM=qemu-arm64-virt
amd64:
	@$(MAKE) ARCH=amd64 PLATFORM=qemu-amd64-q35
run: $(KERNEL_ELF)
	@$(SRCTREE)/tools/run/run.sh $(KERNEL_ELF)
certification:
	@$(MAKE) BUILD_VARIANT=certification ARCH=arm64 PLATFORM=qemu-arm64-virt all

production-gate:
	@$(SRCTREE)/tools/release/check_production_source.sh $(SRCTREE)
	@$(MAKE) BUILD_VARIANT=release ARCH=arm64 PLATFORM=qemu-arm64-virt O=out/release/arm64 clean all
	@$(SRCTREE)/tools/release/check_production_elf.sh $(SRCTREE)/out/release/arm64/$(PROJECT).elf

release: format-check production-gate
	@$(MAKE) BUILD_VARIANT=release ARCH=amd64 PLATFORM=qemu-amd64-q35 O=out/release/amd64 clean all
	@$(SRCTREE)/tools/release/check_production_elf.sh $(SRCTREE)/out/release/amd64/$(PROJECT).elf
	@$(SRCTREE)/tools/release/make_release.sh $(SRCTREE) $(SRCTREE)/out/release $(PROJECT) $(VERSION)
clean:
	@rm -rf out
.PHONY: FORCE
FORCE:

DOC_OUT := $(SRCTREE)/out/doc
.PHONY: doc doc-check

doc:
	@python3 $(SRCTREE)/tools/doc/collect.py --root $(SRCTREE)/src --output $(DOC_OUT)/zilch_design.md --index $(DOC_OUT)/module_index.json

doc-check:
	@python3 $(SRCTREE)/tools/doc/collect.py --root $(SRCTREE)/src --output $(DOC_OUT)/zilch_design.md --index $(DOC_OUT)/module_index.json >/dev/null
