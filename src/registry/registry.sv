// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

package cvm_registry;
endpackage

`define CVM_REGISTRY_SET_SCOPE(LOC) \
    import "DPI-C" context function int cvm_registry_set_scope(int unsigned location); \
    int _unused_cvm_registry_set_scope = cvm_registry_set_scope(LOC);
