SHELL := /bin/sh

ROOT := $(abspath ../../..)
OUT := $(CURDIR)/out
MODE ?= debug
DEFCONFIG := $(CURDIR)/$(MODE)_defconfig
ZEPHYR := $(OUT)/zephyr
BUILD := $(OUT)/build
GUEST_ELF := $(BUILD)/zephyr/zephyr.guest.elf
ZILCH_OUT := $(OUT)/zilch/$(MODE)
ZILCH_ELF := $(ZILCH_OUT)/zilch.elf

.PHONY: fetch guest image run acceptance clean

fetch:
	@./fetch.sh $(ZEPHYR)

guest: $(GUEST_ELF)

$(GUEST_ELF): CMakeLists.txt hypervisor.overlay prj.conf
	@[ -d $(ZEPHYR) ] || { echo "error: run make fetch first" >&2; exit 1; }
	@python_site=$$(nix develop --command python -c 'import site; print(site.getsitepackages()[0])'); \
		ZEPHYR_BASE=$(ZEPHYR) ZEPHYR_MODULES=$(OUT)/modules/hal/cmsis ZEPHYR_TOOLCHAIN_VARIANT=llvm PYTHONPATH=$$python_site \
		nix develop --command cmake -G Ninja -S . -B $(BUILD) -DBOARD=qemu_cortex_a53 \
		-DDTC_OVERLAY_FILE=hypervisor.overlay -DCONF_FILE=prj.conf
	@python_site=$$(nix develop --command python -c 'import site; print(site.getsitepackages()[0])'); \
		PYTHONPATH=$$python_site nix develop --command ninja -C $(BUILD)
	@nix develop --command llvm-strip --strip-debug -o $@ $(BUILD)/zephyr/zephyr.elf

image: guest
	@$(MAKE) -C $(ROOT) O=$(ZILCH_OUT) BUILD_VARIANT=$(MODE) KCONFIG_DEFCONFIG=$(DEFCONFIG) DOMAIN_GUEST_ELF=$(GUEST_ELF)

run: image
	@QEMU_CPUSET=$${QEMU_CPUSET:-4-7} $(MAKE) -C $(ROOT) O=$(ZILCH_OUT) BUILD_VARIANT=$(MODE) KCONFIG_DEFCONFIG=$(DEFCONFIG) run

acceptance: image
	@log=$$(mktemp); status=0; trap 'rm -f "$$log"' EXIT; \
		(sleep 8; printf 'help\r') | QEMU_CPUSET=$${QEMU_CPUSET:-4-7} \
		timeout -k 5s 25s $(ROOT)/tools/run/run.sh \
		$(ZILCH_ELF) >"$$log" 2>&1 || status=$$?; \
		cat "$$log"; \
		test "$$status" = 124; \
		grep -q 'Zilch Zephyr guest booted' "$$log"; \
		grep -Fq 'zilch:~$$' "$$log"; \
		grep -q 'Available commands:' "$$log"

clean:
	@rm -rf $(OUT)
