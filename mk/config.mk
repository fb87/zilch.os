# SPDX-License-Identifier: Apache-2.0
PROJECT ?= zilch
VERSION ?= 0.8.0
BOOT_PROFILE ?= root
BUILD_VARIANT ?= debug
ARCH ?= arm64
CPUS ?= 4
MEMORY_MB ?= 256
LLVM ?= 1
CROSS_COMPILE ?=

VALID_BUILD_VARIANTS := debug release development certification
ifeq ($(filter $(BUILD_VARIANT),$(VALID_BUILD_VARIANTS)),)
$(error BUILD_VARIANT=$(BUILD_VARIANT) is invalid; expected one of $(VALID_BUILD_VARIANTS))
endif

# development and certification remain transitional aliases for debug until CI
# and historical commands migrate to explicit Kconfig defconfigs.
BUILD_PROFILE ?= $(if $(filter release,$(BUILD_VARIANT)),release,debug)
VALID_BUILD_PROFILES := debug release
ifeq ($(filter $(BUILD_PROFILE),$(VALID_BUILD_PROFILES)),)
$(error BUILD_PROFILE=$(BUILD_PROFILE) is invalid; expected debug or release)
endif

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

KCONFIG_DEFCONFIG ?= $(SRCTREE)/configs/$(BUILD_PROFILE)_defconfig
KCONFIG_CONFIG := $(OBJTREE)/.config
KCONFIG_AUTO_CONF := $(OBJTREE)/include/generated/auto.conf
KCONFIG_AUTOCONF_H := $(OBJTREE)/include/generated/autoconf.h
KCONFIG_SOURCES := $(SRCTREE)/Kconfig $(SRCTREE)/src/kernel/Kconfig $(SRCTREE)/src/user/Kconfig $(SRCTREE)/samples/guests/Kconfig

$(KCONFIG_CONFIG) $(KCONFIG_AUTO_CONF) $(KCONFIG_AUTOCONF_H): $(KCONFIG_SOURCES) $(KCONFIG_DEFCONFIG) $(SRCTREE)/tools/config/generate.py
	@python3 $(SRCTREE)/tools/config/generate.py --root $(SRCTREE) --defconfig $(KCONFIG_DEFCONFIG) \
		--config $(KCONFIG_CONFIG) --auto-conf $(KCONFIG_AUTO_CONF) --autoconf-h $(KCONFIG_AUTOCONF_H)

-include $(KCONFIG_AUTO_CONF)

export SRCTREE OBJTREE ARCH PLATFORM BUILD_VARIANT BUILD_PROFILE KCONFIG_DEFCONFIG KCONFIG_CONFIG KCONFIG_AUTO_CONF KCONFIG_AUTOCONF_H CONFIG_DEBUG CONFIG_RELEASE CONFIG_TESTS CONFIG_SELFTEST CONFIG_HYPERVISOR_SELFTEST CONFIG_VERBOSE_DIAGNOSTICS CONFIG_TRACE CONFIG_DEBUG_INFO CONFIG_PRINTK_TIME CONFIG_GUEST_SUPPORT CONFIG_GUEST_TEST_ARM64 CONFIG_GUEST_EXTERNAL CONFIG_GUEST_EMBEDDED_IMAGE CONFIG_GUEST_INTERACTIVE CONFIG_GUEST_ZEPHYR
