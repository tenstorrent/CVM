load("@rules_python//python:pip.bzl", "pip_parse")

def cvm_toolchains1():

    pip_parse(
        name = "cvm_pypi",
        # (Optional) You can set quiet to False if you want to see pip output.
        #quiet = False,
        requirements_lock = "//deps:requirements_lock.txt",
    )
