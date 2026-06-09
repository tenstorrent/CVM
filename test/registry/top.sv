// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

module top();

    import "DPI-C" function void cvm_registry_reset();
    import "DPI-C" function void cvm_registry_reset2();

    initial begin
        cvm_registry_reset();
        cvm_registry_reset2();
        $finish;
    end
endmodule
