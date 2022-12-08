load(
  "@rules_verilator//verilator:repositories.bzl",
  "rules_verilator_dependencies",
  "rules_verilator_toolchains"
)

load(
  "@rules_vcs//vcs:repositories.bzl",
  "rules_vcs_dependencies",
)

def internal_toolchains1():
    rules_verilator_dependencies()
    rules_verilator_toolchains()

    rules_vcs_dependencies()
