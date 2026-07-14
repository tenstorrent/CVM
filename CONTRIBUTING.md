# Contributing to cvm

Thank you for your interest in contributing to `cvm`! This document describes
how to build the project, the standards we expect of contributions, and the
process for submitting changes.

By participating in this project, you agree to abide by our
[Code of Conduct](CODE_OF_CONDUCT.md).

## Getting Started

`cvm` builds with [Bazel](https://bazel.build/) and is developed inside a
container image so the toolchain (Bazel, clang 20, Python 3) is reproducible.

```sh
# Build everything
bazel build //...

# Run the unit tests under test/
bazel test //test/...
```

The `Containerfile` at the repo root defines the reference build environment
(also published as `ghcr.io/tenstorrent/cvm`). Building inside that container is
the supported path.

## Coding Standards

- **Formatting** — C++ is formatted with `clang-format` using the repo's
  `.clang-format`. Run `clang-format` on changed files before committing.
- **Linting** — `clang-tidy` runs against the checks in `.clang-tidy`.
- **Tests** — add or update tests under `test/` for any behavior change, and
  make sure `bazel test //test/...` passes.

## Licensing and SPDX Headers

This project is [REUSE](https://reuse.software) compliant and CI enforces it
with `reuse lint`. Every new file must be covered by a license, either by:

- adding an inline SPDX header to the top of the file:

  ```cpp
  // SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
  // SPDX-License-Identifier: Apache-2.0
  ```

- or, for files that cannot carry a comment header (data files, generated
  templates, etc.), adding an entry to `REUSE.toml`.

Code is licensed under **Apache-2.0**; prose documentation is licensed under
**CC-BY-4.0** (see [LICENSE](LICENSE) and [LICENSE-DOCS](LICENSE-DOCS)). Run
`reuse lint` locally to confirm compliance before opening a pull request.

## Submitting Changes

1. Fork the repository and create a topic branch from the default branch.
2. Make your changes, keeping commits focused and with clear messages.
3. Ensure the build, tests, `clang-format`, and `reuse lint` all pass.
4. Open a pull request describing the motivation and the change. Link any
   related issue.
5. A maintainer will review your PR. Please respond to review feedback and keep
   your branch up to date with the default branch.

## Reporting Issues

- For **bugs and feature requests**, open a GitHub issue with enough detail to
  reproduce or understand the request.
- For **security vulnerabilities**, do **not** open a public issue — follow the
  process in [SECURITY.md](SECURITY.md).
