load(
  "@rules_verilator//verilator:repositories.bzl",
  "rules_verilator_dependencies",
  "rules_verilator_toolchains"
)

def internal_toolchains1():
    rules_verilator_dependencies()
    rules_verilator_toolchains()
