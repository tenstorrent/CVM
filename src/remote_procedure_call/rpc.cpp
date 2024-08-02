#include "cvm/rpc.hpp"

void cvm::rpc::build() {
    quit_.clear();
}

void cvm::rpc::clear() {
    quit_.test_and_set();

    {
        std::lock_guard<std::mutex> guard(remote_calls_mutex_);
        remote_calls_.clear();
    }
}