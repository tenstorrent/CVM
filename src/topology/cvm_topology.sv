package cvm_topology;

  localparam int unsigned nil = '0;

  // VCS doesn't like this
  // let get_location (mod, num) = num < mod.TOTAL ? mod.ID + num : nil;

  // class location #(type T);
  //  static function int unsigned get (T mod, int unsigned num);
  //    return num < mod.TOTAL ? mod.ID + num : nil;
  //  endfunction
  // endclass

`define TOPOLOGY                                     \
    parameter type TOPOLOGY          =     int,      \
    parameter TOPOLOGY topology      =       0

`define TOPOLOGY_CFG                                 \
    .TOPOLOGY(TOPOLOGY),                             \
    .topology(topology)

endpackage
