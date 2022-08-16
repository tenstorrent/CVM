module dut(input clk, input rst);

    typedef struct packed {
        logic[9:0] prn;
        logic[4:0] arn;
    } my_m_ret;

    for (genvar i = 0; i < 8; i++) begin : sub
        my_m_ret m_ret;

        always @(posedge clk) begin
            if (rst) begin
                m_ret.arn <= i;
                m_ret.prn <= 8*i;
            end else begin
                m_ret.prn <= m_ret.prn + 1;
            end
        end
    end

endmodule

module top(input clk);

    `TX_DOMAIN(1, clk)

    int clock_count = 0;
    always @(posedge clk) begin
        clock_count <= clock_count + 1;
        if (clock_count == 0) $display("hello world");
        if (clock_count == 9) $finish;
    end

    logic rst;
    assign rst = clock_count < 5;

    dut dut(.clk, .rst);

    for (genvar i = 0; i < 8; i++) begin
        assign tx_dom_1.m_ret_txns[i].valid = !rst && (dut.sub[i].m_ret.prn % 2) == 0;
        assign tx_dom_1.m_ret_txns[i].data  = dut.sub[i].m_ret;
    end

    import "DPI-C" function void start_monitor();
    initial start_monitor();

endmodule

