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

# BUILD_VARIANT names an OUTPUT TREE, not a profile. Which profile is built
# is decided solely by the selected defconfig -- guest_defconfig is a release
# build despite smoke.sh placing it in the `development` tree, and that is
# fine precisely because the variant carries no profile meaning.
#
# BUILD_PROFILE survives only to pick a DEFAULT defconfig when none is given,
# so `make debug` still means configs/debug_defconfig. It is a convenience
# mapping rather than a competing mechanism, now that selecting a different
# defconfig reliably regenerates the config (see KCONFIG_STAMP below).
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

# Which defconfig produced the generated config, recorded so that SELECTING a
# different one regenerates it.
#
# The rule below depends on $(KCONFIG_DEFCONFIG) as a FILE, so pointing it at
# a different defconfig does not invalidate an already-newer generated config
# -- the tree silently keeps whatever profile it was last built with. That is
# not hypothetical: building a tree with configs/release_defconfig and then
# rebuilding it with configs/debug_defconfig left CONFIG_TESTS unset, and
# tools/verification/smoke.sh carries a manual rm of the generated files to
# work around exactly this. The stamp is rewritten only when the selection
# actually changes, so its timestamp moves on a switch and on nothing else.
KCONFIG_STAMP := $(OBJTREE)/include/generated/defconfig.stamp

$(KCONFIG_CONFIG) $(KCONFIG_AUTO_CONF) $(KCONFIG_AUTOCONF_H): $(KCONFIG_SOURCES) $(KCONFIG_DEFCONFIG) $(KCONFIG_STAMP) $(SRCTREE)/tools/config/generate.py
	@python3 $(SRCTREE)/tools/config/generate.py --root $(SRCTREE) --defconfig $(KCONFIG_DEFCONFIG) \
		--config $(KCONFIG_CONFIG) --auto-conf $(KCONFIG_AUTO_CONF) --autoconf-h $(KCONFIG_AUTOCONF_H)

$(KCONFIG_STAMP): FORCE
	@mkdir -p $(dir $@)
	@printf "%s\n" "$(KCONFIG_DEFCONFIG)" > $@.new
	@cmp -s $@.new $@ || mv $@.new $@
	@rm -f $@.new

-include $(KCONFIG_AUTO_CONF)

export SRCTREE OBJTREE ARCH PLATFORM BUILD_VARIANT BUILD_PROFILE KCONFIG_DEFCONFIG KCONFIG_CONFIG KCONFIG_AUTO_CONF KCONFIG_AUTOCONF_H CONFIG_DEBUG CONFIG_RELEASE CONFIG_TESTS CONFIG_SELFTEST CONFIG_HYPERVISOR_SELFTEST CONFIG_VERBOSE_DIAGNOSTICS CONFIG_TRACE CONFIG_DEBUG_INFO CONFIG_PRINTK_TIME CONFIG_GUEST_SUPPORT CONFIG_GUEST_TEST_ARM64 CONFIG_GUEST_EXTERNAL CONFIG_GUEST_EMBEDDED_IMAGE CONFIG_GUEST_INTERACTIVE CONFIG_GUEST_ZEPHYR
