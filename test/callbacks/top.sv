// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

module top();

    int unsigned counter = 0;

    export "DPI-C" function increment;
    function void increment();
        counter = counter + 1;
    endfunction

    import "DPI-C" function void cvm_callbacks_run_test();

    `CVM_REGISTRY_SET_SCOPE(7)

    initial begin
        cvm_callbacks_run_test();
        if (counter != 3) begin
            $display("FAIL: counter=%0d, expected 3", counter);
            $finish(1);
        end
        $finish;
    end

endmodule
