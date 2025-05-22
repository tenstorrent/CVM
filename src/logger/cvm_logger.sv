package cvm_logger;

    import "DPI-C" cvm_logger_get_verbosity = function int unsigned get_verbosity(string v);
    import "DPI-C" cvm_logger_get_verbosity_from_plusargs = function int unsigned get_verbosity_from_plusargs(string p);
    
endpackage
