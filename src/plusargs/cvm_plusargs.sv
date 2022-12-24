package cvm_plusargs;

    import "DPI-C" cvm_plusargs_get_bool   = function byte    unsigned get_bool    (string plusarg);
    import "DPI-C" cvm_plusargs_get_int32  = function int       signed get_int     (string plusarg);
    import "DPI-C" cvm_plusargs_get_int64  = function longint   signed get_longint (string plusarg);
    import "DPI-C" cvm_plusargs_get_uint64 = function longint unsigned get_ulongint(string plusarg);
    import "DPI-C" cvm_plusargs_get_double = function real             get_real    (string plusarg);
    import "DPI-C" cvm_plusargs_get_string = function string           get_string  (string plusarg);

endpackage
