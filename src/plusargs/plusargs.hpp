// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>
#include <gflags/gflags.h>


namespace cvm {

    namespace plusargs {

        void parse();

        

    }
}

// C interface for plusargs access from external code
extern "C" {
    std::uint8_t  cvm_plusargs_get_bool  (const char* p);
    std::int32_t  cvm_plusargs_get_int32 (const char* p);
    std::uint32_t cvm_plusargs_get_uint32(const char* p);
    std::int64_t  cvm_plusargs_get_int64 (const char* p);
    std::uint64_t cvm_plusargs_get_uint64(const char* p);
    double        cvm_plusargs_get_double(const char* p);
    const char*   cvm_plusargs_get_string(const char* p);
}
