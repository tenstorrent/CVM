module dut #(
    `TRANSACTIONS_DUT_OUTPUT_PARAMS
) (
    input clk,
    input rst,
    `TRANSACTIONS_DUT_OUTPUT_PORTS
);

    logic [31:0] count;
    always @(posedge clk) count <= rst ? '0 : (count + 1);

    int unsigned loc = cvm_topology::nil;
    always @(posedge clk) begin
        if (rst) begin
            loc = cvm_topology::get_location(topology_pkg::mods.TOP.CLUSTER.CORE.ID, 0);
        end
    end

    for (genvar i = 0; i < 8; i++) begin
        always @(posedge clk) begin
            automatic logic[31:0] c = count - 1;
            pkts[i].valid          <= count > 0;
            pkts[i].data.location  <= loc;
            pkts[i].data.num       <= i;
            pkts[i].data.num1[0][0] <= 4'b1;
            pkts[i].data.num1[0][1] <= 4'b100;
            pkts[i].data.num1[1][0] <= 4'b011;
            pkts[i].data.num1[1][1] <= 4'b101;
            pkts[i].data.num2[0][0][0] <= 13'h1;
            pkts[i].data.num2[0][1][0] <= 13'h1000;
            pkts[i].data.num2[1][0][1] <= 13'h0011;
            pkts[i].data.num2[2][1][2] <= 13'h1100;
            pkts[i].data.x256      <= 256'(1) << 255 | 256'(c);
            pkts[i].data.x54       <=  54'(1) <<  53 | 54'(c);
            pkts[i].data.valid1    <= c == 4;
            pkts[i].data.optional1 <= 8'(c);
            pkts[i].data.valid2    <= c == 5;
            pkts[i].data.optional2 <= 8'(c);
            pkts[i].data.valid3a   <= 64'(c == 6 || c == 7);
            pkts[i].data.valid3b   <= 64'(c == 7);
            pkts[i].data.optional3 <= 512'(c);
        end
    end

    assign ctxs[0].valid           = count == 10;
    assign ctxs[0].data.location   = loc;
    assign ctxs[0].data.dummy[0]   = 1'b1;

endmodule

module dut2 #(
    `TRANSACTIONS_DUT2_OUTPUT_PARAMS
) (
    input clk,
    input rst,
    `TRANSACTIONS_DUT2_OUTPUT_PORTS
);

    int unsigned loc = cvm_topology::nil;
    always@ (posedge clk) begin
        if (rst) begin
            loc = cvm_topology::get_location(topology_pkg::mods.TOP.CLUSTER.CORE.ID, 0);
        end
    end

    assign pkt2s[0].valid             = loc != cvm_topology::nil;
    assign pkt2s[0].data.location     = loc;
    assign pkt2s[0].data.dummy2       = 3;
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

    dut #(
        `TRANSACTIONS_DUT_SOURCE_PARAMS(0)
    ) dut (
        .clk,
        .rst,
        `TRANSACTIONS_DUT_SOURCE_PORTS(1, 0, 0)
    );

    for (genvar p = 0; p < 2; p++) begin
        dut2 #(
            `TRANSACTIONS_DUT2_SOURCE_PARAMS(0)
        ) dut2 (
            .clk,
            .rst,
            `TRANSACTIONS_DUT2_SOURCE_PORTS(1, p, 0)
        );
    end

    for (genvar p = 0; p < 1; p++) begin
        dut2 #(
            `TRANSACTIONS_DUT2_SOURCE_PARAMS(1)
        ) dut3 (
            .clk,
            .rst,
            `TRANSACTIONS_DUT2_SOURCE_PORTS(1, p, 1)
        );
    end

    import "DPI-C" function void start_checker();
    import "DPI-C" function void end_checker();
    initial start_checker();
    final   end_checker();

endmodule

