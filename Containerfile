# Minimal CI container for cvm. Provides Bazel 6.5.0 + Bazel 7.7.1, clang 20,
# Python 3, m4 / flex / bison, libatomic (with the .so symlink ld expects).
FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

# Apt-installed deps. `libgcc-12-dev` + `libstdc++-12-dev` ship
# /usr/lib/gcc/x86_64-linux-gnu/12/libatomic.so, which clang picks up via its
# default search path. That's the symlink whose absence forced the hacks the
# previous Rocky-Linux image required.
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        git \
        gnupg \
        python3 \
        python3-pip \
        python3-venv \
        libgcc-12-dev \
        libstdc++-12-dev \
        libatomic1 \
        liblz4-dev \
        m4 \
        flex \
        bison \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# LLVM apt repo -> clang 20 + lld 20.
RUN install -d /etc/apt/keyrings \
    && curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key \
        -o /etc/apt/keyrings/llvm-snapshot.asc \
    && echo "deb [signed-by=/etc/apt/keyrings/llvm-snapshot.asc] https://apt.llvm.org/bookworm/ llvm-toolchain-bookworm-20 main" \
        > /etc/apt/sources.list.d/llvm-20.list \
    && apt-get update && apt-get install -y --no-install-recommends \
        clang-20 \
        lld-20 \
    && rm -rf /var/lib/apt/lists/* \
    && update-alternatives --install /usr/bin/clang clang /usr/bin/clang-20 100 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-20 100

# Bazel 6.5.0 + Bazel 7.7.1. /usr/local/bin/bazel defaults to 6.5.0 to match
# the build-bazel6-top job; bazel-7 maps to 7.7.1.
RUN curl -fsSL -o /usr/local/bin/bazel-6.5.0 \
        https://github.com/bazelbuild/bazel/releases/download/6.5.0/bazel-6.5.0-linux-x86_64 \
    && curl -fsSL -o /usr/local/bin/bazel-7.7.1 \
        https://github.com/bazelbuild/bazel/releases/download/7.7.1/bazel-7.7.1-linux-x86_64 \
    && chmod +x /usr/local/bin/bazel-6.5.0 /usr/local/bin/bazel-7.7.1 \
    && ln -sf /usr/local/bin/bazel-6.5.0 /usr/local/bin/bazel \
    && ln -sf /usr/local/bin/bazel-7.7.1 /usr/local/bin/bazel-7

WORKDIR /work
