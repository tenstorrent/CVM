package cvm_topology;

    localparam longint unsigned nil = '0;
    import "DPI-C" cvm_topology_get_location = function longint unsigned get_location(int unsigned mod, int unsigned id);

endpackage
