module dut(input clk, input rst);

    typedef struct packed {
        logic[2:0]  bar;
        logic[53:0] foo;
        logic[9:0]  prn;
        logic[4:0]  arn;
    } my_m_ret;

    for (genvar i = 0; i < 8; i++) begin : sub
        my_m_ret m_ret;

        always @(posedge clk) begin
            if (rst) begin
                m_ret.arn <= i;
                m_ret.prn <= 8*i;
            end else begin
                m_ret.prn <= m_ret.prn + 1;
                m_ret.foo <= 54'(1) << 53 | 54'(1);
                m_ret.bar <=  3'(1) <<  2 |  3'(1);
            end
        end
    end

endmodule

module top(
`ifdef TB_EXTERNAL_CLOCK
    input clk
`endif
);

`ifndef TB_EXTERNAL_CLOCK
    logic clk;
    initial begin
        clk = '1;
        forever #5 clk = !clk;
    end
`endif

    `TRANSACTIONS_DOMAIN(1, clk)

    int clock_count = 0;
    always @(posedge clk) begin
        clock_count <= clock_count + 1;
        if (clock_count == 0) $display("hello world");
        if (clock_count == 15) $finish;
    end

    logic rst;
    assign rst = clock_count < 5;

    dut dut(.clk, .rst);

    for (genvar i = 0; i < 8; i++) begin
        always @(posedge clk) begin
            $display("clock_count:%0d rst:%0b dut.sub[%0d].m_ret.prn %0h", clock_count, rst, i, dut.sub[i].m_ret.prn);
        end
        assign tx_dom_1.m_rets[i].valid = !rst && (dut.sub[i].m_ret.prn % 2) == 0;
        assign tx_dom_1.m_rets[i].data.arn  = dut.sub[i].m_ret.arn;
        assign tx_dom_1.m_rets[i].data.prn  = dut.sub[i].m_ret.prn;
        assign tx_dom_1.m_rets[i].data.foo  = dut.sub[i].m_ret.foo;
        assign tx_dom_1.m_rets[i].data.bar  = dut.sub[i].m_ret.bar;
    end

    import "DPI-C" function void start_monitor();
    initial start_monitor();

endmodule

