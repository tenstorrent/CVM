module top(
  input logic clk,
  input logic rst
);

    int clock_count = 0;
    always @(posedge clk) begin
        clock_count <= clock_count + 1;
    end

    logic [2:0] out_state;
    cvm_lfsr #(
        .WIDTH(3),
        .NUM_TAPS(2),
        .TAPS({32'd2, 32'd0}) // random numbers
    ) lfsr (
        .clk,
        .rst,
        .seed(3'b001),
        .step(1'b1),
        .out_state
    );

    import "DPI-C" function void forward_lfsr_value(int unsigned state);
    always @(posedge clk) begin
        if (!rst) begin
            forward_lfsr_value({29'd0, out_state});
        end
    end

endmodule
