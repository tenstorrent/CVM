#! /usr/bin/env bash
# SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
# SPDX-License-Identifier: Apache-2.0

# Compares two port specs. Usage: diff.sh <expected> <actual> <why...>
# bazel tokenizes a rule's args, so <why> spans every remaining argument.
set -o pipefail

expected=$1
actual=$2
shift 2

if diff -u "$expected" "$actual"; then
    exit 0
fi
echo
echo "diff.sh: $*"
exit 1
