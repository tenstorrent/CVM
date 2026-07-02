// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

module plusargs_test();

    int testflaginfile = '0;
    bit testsetbool = '0;
    initial begin

        testflaginfile = cvm_plusargs::get_int("testflaginfile");
        assert(testflaginfile == 42) else $error("unexpected");

        testsetbool = cvm_plusargs::get_bool("testsetbool") != '0;
        assert(testsetbool == '1) else $error("unexpected");

        $finish;

    end

endmodule
