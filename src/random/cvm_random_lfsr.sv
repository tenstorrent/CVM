// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

module cvm_lfsr #(

    parameter int unsigned WIDTH                = 32'd2,
    parameter int unsigned NUM_TAPS             = 32'd2,
    parameter logic [NUM_TAPS-1:0] [31:0]TAPS  = '{NUM_TAPS{32'd0}}
)
(
    input  logic             clk,
    input  logic             rst,
    input  logic [WIDTH-1:0] seed,
    input  logic             step,
    output logic [WIDTH-1:0] out_state
);

    if (WIDTH < 2) $error("Error: LFSR width must be greater than 2.");
    if ((NUM_TAPS < 2) || (NUM_TAPS > WIDTH)) $error("Error: LFSR number of taps must be greater than 2 and within width.");

    for (genvar i = 1; i < NUM_TAPS; ++i) begin
      if (TAPS[i] <= TAPS[i-1]) $error ("Error: LFSR taps must be in increasing order.");
    end

    for (genvar i = 0; i < NUM_TAPS; ++i) begin
      if (TAPS[i] >= WIDTH) $error ("Error: LFSR tap must fit within LFSR width.");
    end

    logic [WIDTH-1:0] state;
    assign out_state = state;

    logic [WIDTH-1:0] next_state;
    always_comb begin
      automatic logic shift_in = state[TAPS[0]];
      for (int unsigned i = 1; i < NUM_TAPS; ++i) begin
        shift_in ^= state[TAPS[i]];
      end
      next_state = {shift_in, state[WIDTH-1:1]};
    end

    always_ff @(posedge clk) begin
      if (rst) begin
        state <= seed;
      end
      else if (step) begin
        state <= next_state;
      end
    end

endmodule
