# SPDX-License-Identifier: Apache-2.0
PROJECT ?= zilch
VERSION ?= 0.8.0
BOOT_PROFILE ?= root
BUILD_VARIANT ?= development
ARCH ?= arm64
CPUS ?= 4
MEMORY_MB ?= 256
LLVM ?= 1
CROSS_COMPILE ?=

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

SUPPORTED_ARCHES := arm64 amd64
SUPPORTED_arm64_PLATFORMS := qemu-arm64-virt
SUPPORTED_amd64_PLATFORMS := qemu-amd64-q35
ifeq ($(filter $(ARCH),$(SUPPORTED_ARCHES)),)
$(error Unsupported ARCH=$(ARCH))
endif
ifeq ($(filter $(PLATFORM),$(SUPPORTED_$(ARCH)_PLATFORMS)),)
$(error PLATFORM=$(PLATFORM) is invalid for ARCH=$(ARCH))
endif

O ?= out/build/$(ARCH)/$(PLATFORM)/$(BUILD_VARIANT)
SRCTREE := $(CURDIR)
OBJTREE := $(abspath $(O))
export SRCTREE OBJTREE ARCH PLATFORM BUILD_VARIANT CONFIG_SELFTEST CONFIG_HYPERVISOR_SELFTEST CONFIG_VERBOSE_DIAGNOSTICS
