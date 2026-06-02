package cvm_registry;

    import "DPI-C" context cvm_set_scope = function void set_scope( int unsigned location, string scope);

endpackage
`define CVM_REGISTRY_SET_SCOPE(location) cvm_registry::set_scope(location, $sformatf("%m"));

