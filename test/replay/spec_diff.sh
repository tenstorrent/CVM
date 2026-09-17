#! /usr/bin/env bash
# SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
# SPDX-License-Identifier: Apache-2.0

# Compares two port specs. Usage: spec_diff.sh <expected> <actual> <why>
set -o pipefail

if diff -u "$1" "$2"; then
    exit 0
fi
echo
echo "spec_diff.sh: $3"
exit 1
