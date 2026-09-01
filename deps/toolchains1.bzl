load("@rules_python//python:pip.bzl", "pip_parse")
load("@rules_python//python:repositories.bzl", "python_register_toolchains")

# Hermetic interpreter used ONLY to resolve pip_parse wheels (same pattern as
# rv_tester's infra/bazel/dependencies.bzl).
#
# The CI container's system python3 is 3.13 (Debian trixie), but rules_python
# 0.11's pip bootstrap ships a setuptools whose pkg_resources calls
# pkgutil.ImpImporter -- an API removed in Python 3.12 -- so resolving
# @cvm_pypi against system python dies with
#   AttributeError: module 'pkgutil' has no attribute 'ImpImporter'
# We point pip_parse at a hermetic CPython 3.9 (matching the toolchain
# MODULE.bazel registers for bzlmod). Referencing the platform repo directly
# (rather than the @python3_9//:defs.bzl alias) avoids an extra WORKSPACE
# load stage; CI is x86_64 linux.
_PY_INTERPRETER = "@python3_9_x86_64-unknown-linux-gnu//:python"

def cvm_toolchains1():

    # register_toolchains = False: the interpreter is only for pip resolution;
    # don't register cvm's hermetic Python into downstream consumers' builds.
    # ignore_root_user_error: CI container jobs run as root, and 0.11.0's
    # read-only-installation check hard-fails for root otherwise.
    python_register_toolchains(
        name = "python3_9",
        python_version = "3.9",
        ignore_root_user_error = True,
        register_toolchains = False,
    )

    pip_parse(
        name = "cvm_pypi",
        # (Optional) You can set quiet to False if you want to see pip output.
        #quiet = False,
        python_interpreter_target = _PY_INTERPRETER,
        requirements_lock = "@cvm//deps:requirements_lock.txt",
    )
