# SPDX-License-Identifier: Apache-2.0
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

ifeq ($(ARCH),arm64)
TARGET_FLAGS := $(if $(filter 1,$(LLVM)),--target=aarch64-none-elf,)
ARCH_FLAGS := -march=armv8-a -mgeneral-regs-only
LD_EMULATION := -m aarch64elf
KERNEL_LDSCRIPT := $(SRCTREE)/src/arch/arm64/kernel.ld
else
TARGET_FLAGS := $(if $(filter 1,$(LLVM)),--target=x86_64-none-elf,)
ARCH_FLAGS := -m64 -mno-red-zone -mcmodel=kernel -mno-sse -mno-sse2 -mno-mmx
LD_EMULATION := -m elf_x86_64
KERNEL_LDSCRIPT := $(SRCTREE)/src/arch/amd64/kernel.ld
endif

COMMON_WARNINGS := -Wall -Wextra -Werror -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wundef -Wcast-align -Wcast-qual -Wformat=2 -Wimplicit-fallthrough
FREESTANDING_CXXFLAGS := -std=c++20 -ffreestanding -nostdinc++ -fno-builtin -fno-common -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit -fno-unwind-tables -fno-asynchronous-unwind-tables -fdata-sections -ffunction-sections $(COMMON_WARNINGS)
export CC CXX LD NM OBJCOPY OBJDUMP READELF TARGET_FLAGS ARCH_FLAGS LD_EMULATION COMMON_WARNINGS FREESTANDING_CXXFLAGS
