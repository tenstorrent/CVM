#include "cvm/rpc.hpp"

void cvm::rpc::build() {
    quit_.clear();
}

void cvm::rpc::clear() {
    cvm::log(cvm::DEBUG, "[remote_procedure_call] clearing remote calls...\n");
    quit_.test_and_set();

    {
        std::lock_guard<std::mutex> guard(remote_calls_mutex_);
        remote_calls_.clear();
    }
}