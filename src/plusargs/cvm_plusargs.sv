// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

package cvm_plusargs;

    import "DPI-C" cvm_plusargs_get_bool   = function byte    unsigned get_bool    (string plusarg);
    import "DPI-C" cvm_plusargs_get_int32  = function int       signed get_int     (string plusarg);
    import "DPI-C" cvm_plusargs_get_int64  = function longint   signed get_longint (string plusarg);
    import "DPI-C" cvm_plusargs_get_uint64 = function longint unsigned get_ulongint(string plusarg);
    import "DPI-C" cvm_plusargs_get_double = function real             get_real    (string plusarg);
    import "DPI-C" cvm_plusargs_get_string_bytes_1024 = function void get_string_bytes_1024(string plusarg, output byte unsigned str_buffer[1024]);

endpackage