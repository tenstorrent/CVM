// FIXME generate
package transactions;

    typedef enum int {
        MSG_NUMBER_M_RET = 0
    } message_number;

    typedef struct packed {
        logic[9:0] prn;
        logic[4:0] arn;
    } m_ret;

    typedef struct packed {
        m_ret data ;
        logic valid;
    } m_ret_txn;

    typedef struct packed {
        m_ret_txn[7:0] m_ret_txns;
    } domain_1;

    typedef struct packed {
        logic[31:0] count;
    } msg;

    typedef struct packed {
        msg   data;
        logic valid;
    } msg_txn;

endpackage

module transactions_messenger #(
    type T = logic,
    transactions::message_number N = 0
) (
    input clk,
    input valid,
    input [$bits(T)-1:0] message
);

    localparam int B = ($bits(T)+7)/8;

    import "DPI-C" function void transactions_message(transactions::message_number num, byte unsigned message[B-1:0]);

    always @(posedge clk) begin
        if (valid) begin
            automatic byte unsigned message_unpacked[B];
            for (int i = 0; i < B-1; i++) begin
                message_unpacked[i] = message[8*i +: 8];
            end
            message_unpacked[B-1] = (8)'(message[8*(B-1) +: $bits(T) % 8]);
            transactions_message(N, message_unpacked);
        end
    end

endmodule

module transactions_domain_1(
    input                        clk,
    input transactions::domain_1 tx
);

    for (genvar i = 0; i < $size(tx.m_ret_txns); i++) begin
        transactions_messenger #(transactions::m_ret, transactions::MSG_NUMBER_M_RET) m_ret_messenger (clk, tx.m_ret_txns[i].valid, tx.m_ret_txns[i].data);
    end

endmodule

`define TX_DOMAIN(domain, clock)                   \
    transactions::domain_``domain tx_dom_``domain; \
    transactions_domain_``domain                   \
        transactions_domain_``domain (             \
            .clk(clock),                           \
            .tx(tx_dom_``domain),                  \
            .*                                     \
        );
