`define TOPOLOGY                                     \
    parameter type TOPOLOGY          =     int,      \
    parameter TOPOLOGY topology      =       0

`define TOPOLOGY_CFG                                 \
    .TOPOLOGY(TOPOLOGY),                             \
    .topology(topology)
