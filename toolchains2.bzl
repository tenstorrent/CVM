load("@cvm_pypi//:requirements.bzl", "install_deps")

def cvm_toolchains2():
    # Initialize repositories for all packages in requirements_lock.txt.
    install_deps()
