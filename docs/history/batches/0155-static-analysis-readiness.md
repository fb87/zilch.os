# Batch 0155: static-analysis readiness

The release checklist now has an executable inventory gate for the required
Clang static-analysis tools. `make static-analysis-tools-check` records tool
paths and versions when available and emits an explicit deviation when
`scan-build` or `clang-tidy` is absent. TST-031 and TST-032 remain open for
the actual clean/profile runs; the missing-tool limitation is no longer
implicit.
