load("@rules_python//python:pip.bzl", "pip_parse")
load("@rules_python//python:repositories.bzl", "python_register_toolchains")

_PY_INTERPRETER = "@python3_9_x86_64-unknown-linux-gnu//:python"
def cvm_toolchains1():

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
