module dut(input clk, input rst);

    typedef struct packed {
        logic[2:0]   num;
        logic[255:0] x256;
        logic[53:0]  x54;
    } my_pkt;

    logic [7:0] count;
    always @(posedge clk) count <= rst ? '0 : (count + 1);

    for (genvar i = 0; i < 8; i++) begin : sub
        my_pkt pkt;

        always @(posedge clk) begin
            pkt.num  <= i;
            pkt.x256 <= 256'(1) << 255 | 256'(count);
            pkt.x54  <=  54'(1) <<  53 |  54'(count);
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
    end

    logic rst;
    assign rst = clock_count < 5;

    dut dut(.clk, .rst);

    for (genvar i = 0; i < 8; i++) begin
        assign tx_dom_1.pkts[i].valid = clock_count >= 6;
        assign tx_dom_1.pkts[i].data.num  = dut.sub[i].pkt.num ;
        assign tx_dom_1.pkts[i].data.x256 = dut.sub[i].pkt.x256 ;
        assign tx_dom_1.pkts[i].data.x54  = dut.sub[i].pkt.x54;
        assign tx_dom_1.pkts[i].location  = topology_pkg::to_loc(topology_pkg::t.CORE, 0);
    end
    assign tx_dom_1.ctxs[0].valid      = clock_count == 15;
    assign tx_dom_1.ctxs[0].data.dummy = 1'b1;
    assign tx_dom_1.ctxs[0].location   = topology_pkg::to_loc(topology_pkg::t.CORE, 0);

    import "DPI-C" function void start_checker();
    initial start_checker();

endmodule

