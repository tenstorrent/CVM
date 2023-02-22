module registry_test();

    import "DPI-C" function void cvm_registry_reset();

    initial begin
        cvm_registry_reset();
        $finish;
    end
endmodule
