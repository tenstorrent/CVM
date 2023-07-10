package cvm_logger;

    parameter ERROR  =   0,
              NONE   =   1,
              LOW    = 100,
              MEDIUM = 200,
              HIGH   = 300,
              FULL   = 400,
              DEBUG  = 500;

    parameter int str_to_verbosity[string] = '{
              "NONE"   : NONE,
              "LOW"    : LOW,
              "MEDIUM" : MEDIUM,
              "HIGH"   : HIGH,
              "FULL"   : FULL,
              "DEBUG"  : DEBUG };

endpackage
