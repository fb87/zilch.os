# SPDX-License-Identifier: Apache-2.0
CLANG_FORMAT ?= clang-format
FORMAT_FILES := $(shell find include src tools tests -type f \( -name '*.cc' -o -name '*.hh' -o -name '*.tt' \))
DOC_OUT := $(SRCTREE)/out/doc

.PHONY: format format-check abi-check abi-headers-check ubsan-check stack-usage-check host-tests production-gate release doc doc-check boundary-check binary-permissions-check reproducible-check static-analysis-tools-check
format:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo 'error: $(CLANG_FORMAT) not found'; exit 1; }
	@$(CLANG_FORMAT) -i $(filter %.cc %.hh,$(FORMAT_FILES))
	@for file in $(filter %.tt,$(FORMAT_FILES)); do $(CLANG_FORMAT) -i --assume-filename="$${file%.tt}.cc" "$$file"; done

format-check:
	@command -v $(CLANG_FORMAT) >/dev/null 2>&1 || { echo 'error: $(CLANG_FORMAT) not found'; exit 1; }
	@failed=0; for file in $(FORMAT_FILES); do case "$$file" in *.tt) extra="--assume-filename=$${file%.tt}.cc";; *) extra="";; esac; $(CLANG_FORMAT) $$extra --dry-run --Werror "$$file" || failed=1; done; exit $$failed

abi-check:
	@CXX=$(CXX) $(SRCTREE)/tools/abi/check_layout.sh $(SRCTREE)

abi-headers-check:
	@CXX=$(CXX) $(SRCTREE)/tools/abi/check_headers.sh $(SRCTREE)

ubsan-check:
	@CXX=$(CXX) $(SRCTREE)/tools/abi/check_ubsan.sh $(SRCTREE)

stack-usage-check:
	@sh $(SRCTREE)/tools/release/check_stack_usage.sh $(or $(STACK_USAGE_ROOT),$(SRCTREE)/out/release/arm64)

boundary-check: abi-headers-check
	@sh $(SRCTREE)/tools/release/check_source_boundaries.sh $(SRCTREE)
	@$(SRCTREE)/tools/release/check_guest_boundary.sh $(SRCTREE)
	@$(SRCTREE)/tools/release/check_user_kernel_boundary.sh $(SRCTREE)
	@$(SRCTREE)/tools/release/check_build_boundaries.sh $(SRCTREE)

binary-permissions-check:
	@sh $(SRCTREE)/tools/release/check_section_permissions.sh $(or $(ELF),$(SRCTREE)/out/build/arm64/qemu-arm64-virt/release/$(PROJECT).elf)

reproducible-check:
	@set -eu; root=$(SRCTREE)/out/reproducible; trap 'rm -rf "$$root"' EXIT HUP INT TERM; \
	for target in "arm64 qemu-arm64-virt" "amd64 qemu-amd64-q35"; do \
		set -- $$target; arch=$$1; platform=$$2; \
		SOURCE_DATE_EPOCH=0 LC_ALL=C TZ=UTC $(MAKE) BUILD_VARIANT=release ARCH=$$arch PLATFORM=$$platform O=$$root/$$arch/a clean all; \
		SOURCE_DATE_EPOCH=0 LC_ALL=C TZ=UTC $(MAKE) BUILD_VARIANT=release ARCH=$$arch PLATFORM=$$platform O=$$root/$$arch/b clean all; \
		$(SRCTREE)/tools/release/check_reproducible_build.sh $$root/$$arch/a $$root/$$arch/b $$arch; \
		done

static-analysis-tools-check:
	@sh $(SRCTREE)/tools/release/check_static_analysis_tools.sh $(SRCTREE)/out/reports/static-analysis-tools.txt

host-tests: abi-check abi-headers-check
	@echo 'Host tests: PASS'

production-gate: abi-check boundary-check
	@$(SRCTREE)/tools/release/check_production_source.sh $(SRCTREE)
	@$(MAKE) BUILD_VARIANT=release ARCH=arm64 PLATFORM=qemu-arm64-virt O=out/release/arm64 clean
	@$(MAKE) BUILD_VARIANT=release ARCH=arm64 PLATFORM=qemu-arm64-virt O=out/release/arm64 all
	@$(SRCTREE)/tools/release/check_production_elf.sh $(SRCTREE)/out/release/arm64/$(PROJECT).elf
	@sh $(SRCTREE)/tools/release/check_section_permissions.sh $(SRCTREE)/out/release/arm64/$(PROJECT).elf
	@sh $(SRCTREE)/tools/release/check_stack_usage.sh $(SRCTREE)/out/release/arm64

release: format-check production-gate
	@$(MAKE) BUILD_VARIANT=release ARCH=amd64 PLATFORM=qemu-amd64-q35 O=out/release/amd64 clean
	@$(MAKE) BUILD_VARIANT=release ARCH=amd64 PLATFORM=qemu-amd64-q35 O=out/release/amd64 all
	@$(SRCTREE)/tools/release/check_production_elf.sh $(SRCTREE)/out/release/amd64/$(PROJECT).elf
	@sh $(SRCTREE)/tools/release/check_section_permissions.sh $(SRCTREE)/out/release/amd64/$(PROJECT).elf
	@sh $(SRCTREE)/tools/release/check_stack_usage.sh $(SRCTREE)/out/release/amd64
	@$(SRCTREE)/tools/release/make_release.sh $(SRCTREE) $(SRCTREE)/out/release $(PROJECT) $(VERSION)

doc:
	@python3 $(SRCTREE)/tools/doc/collect.py --root $(SRCTREE)/src --output $(DOC_OUT)/zilch_design.md --index $(DOC_OUT)/module_index.json

doc-check:
	@python3 $(SRCTREE)/tools/doc/collect.py --root $(SRCTREE)/src --output $(DOC_OUT)/zilch_design.md --index $(DOC_OUT)/module_index.json >/dev/null
	@$(SRCTREE)/tools/doc/check_layout.sh $(SRCTREE)
