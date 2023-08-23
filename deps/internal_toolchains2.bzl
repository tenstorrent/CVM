load("@rules_m4//m4:m4.bzl", "m4_register_toolchains")
load("@rules_flex//flex:flex.bzl", "flex_register_toolchains")
load("@rules_bison//bison:bison.bzl", "bison_register_toolchains")

def internal_toolchains2():
    m4_register_toolchains(extra_copts=["-Wno-error=sign-compare", "-Wno-error=unused-parameter"])
    flex_register_toolchains(extra_copts=["-Wno-error=misleading-indentation", "-Wno-error=pointer-sign"])
    bison_register_toolchains(extra_copts=["-Wno-error=misleading-indentation", "-Wno-error=sign-compare" , "-Wno-error=unused-parameter", "-Wno-error=unused-but-set-variable"])
