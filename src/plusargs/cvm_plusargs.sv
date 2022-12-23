package cvm_plusargs;

    import "DPI-C" function byte    unsigned cvm_plusargs_get_bool(string plusarg);
    import "DPI-C" function int       signed cvm_plusargs_get_int32(string plusarg);
    import "DPI-C" function longint   signed cvm_plusargs_get_int64(string plusarg);
    import "DPI-C" function longint unsigned cvm_plusargs_get_uint64(string plusarg);
    import "DPI-C" function real             cvm_plusargs_get_double(string plusarg);
    import "DPI-C" function string           cvm_plusargs_get_string(string plusarg);

    function bit              get_bool    (string plusarg); return cvm_plusargs_get_bool(plusarg) != '0; endfunction
    function int signed       get_int     (string plusarg); return cvm_plusargs_get_int32(plusarg); endfunction
    function longint signed   get_longint (string plusarg); return cvm_plusargs_get_int64(plusarg); endfunction
    function longint unsigned get_ulongint(string plusarg); return cvm_plusargs_get_uint64(plusarg); endfunction
    function real             get_real    (string plusarg); return cvm_plusargs_get_double(plusarg); endfunction
    function string           get_string  (string plusarg); return cvm_plusargs_get_string(plusarg); endfunction

endpackage
