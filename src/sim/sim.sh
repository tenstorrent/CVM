#! /usr/bin/env bash
# SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
# SPDX-License-Identifier: Apache-2.0

# Runs a simulation and fails on any $error as well as on a non-zero exit.
# $error alone does not stop a simulator or change its exit status, so a
# library's own assertions would otherwise print and be ignored.
set -o pipefail

out=$("$@" 2>&1)
status=$?
printf '%s\n' "$out"

if [[ $status -ne 0 ]]; then
    exit $status
fi
if grep -qE '%Error|^FAIL:' <<< "$out"; then
    echo "sim.sh: simulation reported an error"
    exit 1
fi
