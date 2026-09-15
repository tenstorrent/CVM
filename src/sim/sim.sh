#! /usr/bin/env bash
# SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
# SPDX-License-Identifier: Apache-2.0

# Runs a simulation and fails on any $error as well as on a non-zero exit.
# $error alone does not stop a simulator or change its exit status, so a
# library's own assertions would otherwise print and be ignored.
set -o pipefail

# A scenario whose failure is the point: the simulation must exit non-zero.
expect_failure=0
args=()
for a in "$@"; do
    if [[ $a == --expect-failure ]]; then
        expect_failure=1
    else
        args+=("$a")
    fi
done

out=$("${args[@]}" 2>&1)
status=$?
printf '%s\n' "$out"

if [[ $expect_failure -eq 1 ]]; then
    if [[ $status -eq 0 ]]; then
        echo "sim.sh: expected a non-zero exit, got 0"
        exit 1
    fi
    exit 0
fi
if [[ $status -ne 0 ]]; then
    exit $status
fi
if grep -qE '%Error|^FAIL:' <<< "$out"; then
    echo "sim.sh: simulation reported an error"
    exit 1
fi
