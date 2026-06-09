// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/random.hpp"
#include <gflags/gflags.h>

template <typename TYPE>
TYPE get_flag(const char* p) {

    gflags::CommandLineFlagInfo info;
    bool found = gflags::GetCommandLineFlagInfo(p, &info);
    assert(found && "Plusarg not found");
    return *((TYPE *)info.flag_ptr);

}

namespace cvm {
    namespace rand {
        std::mt19937 gen;
    }
}

void cvm::rand::seed(uint64_t seed) {
    gen.seed(seed);
    lcg::state = seed;
}

template <typename T>
T cvm::rand::get(std::string flag) {
    cvm::rand::uniform_dist<T> uniform(flag);
    return uniform();
}

extern "C" {

    std::uint32_t cvm_rand_get(const char* p) {
        static std::string s;
        s = get_flag<std::string>(p);
        return cvm::rand::get<uint32_t>(s);
    }

}
