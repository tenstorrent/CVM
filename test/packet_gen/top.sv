module dut #(
    `TRANSACTIONS_DUT_OUTPUT_PARAMS
) (
    input clk,
    input rst,
    `TRANSACTIONS_DUT_OUTPUT_PORTS
);

    logic [7:0] count;
    always @(posedge clk) count <= rst ? '0 : (count + 1);

    int unsigned loc = cvm_topology::nil;
    always @(posedge clk) begin
        if (rst) begin
            loc = cvm_topology::get_location(topology_pkg::mods.TOP.CLUSTER.CORE.ID, 0);
        end
    end

    for (genvar i = 0; i < 8; i++) begin
        always @(posedge clk) begin
            pkts[i].valid         <= count > 0;
            pkts[i].data.location <= loc;
            pkts[i].data.num      <= i;
            pkts[i].data.x256     <= 256'(1) << 255 | (256'(count)-1);
            pkts[i].data.x54      <=  54'(1) <<  53 | (54'(count)-1);
        end
    end

    assign ctxs[0].valid           = count == 10;
    assign ctxs[0].data.location   = loc;
    assign ctxs[0].data.dummy      = 1'b1;

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

        dut2 #(
            `TRANSACTIONS_DUT2_SOURCE_PARAMS(1)
        ) dut3 (
            .clk,
            .rst,
            `TRANSACTIONS_DUT2_SOURCE_PORTS(1, p, 1)
        );
    end

    import "DPI-C" function void start_checker();
    initial start_checker();

endmodule

