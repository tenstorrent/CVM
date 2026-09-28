// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <cinttypes>
#include <gflags/gflags.h>
#include <cassert>
#include <iostream>
#include <cstring>
#include <set>
#include <string>

// Storage size of each type name reported by gflags::FlagValue::TypeName().
static std::size_t gflags_size(const std::string& type) {
    if (type == "bool")                      return sizeof(bool);
    if (type == "int32" || type == "uint32") return sizeof(std::int32_t);
    if (type == "int64" || type == "uint64") return sizeof(std::int64_t);
    if (type == "double")                    return sizeof(double);
    if (type == "string")                    return sizeof(std::string);
    return 0;
}

template <typename TYPE>
TYPE get(const char* p) {

    gflags::CommandLineFlagInfo info;
    bool found = gflags::GetCommandLineFlagInfo(p, &info);
    if (!found) {
        std::cerr << "Error: Plusarg not found - " << p << std::endl;
        assert(false);  // Force assertion failure after printing
    }

    const std::size_t defined_size = gflags_size(info.type);
    if (sizeof(TYPE) > defined_size) {
        std::cerr << "Error: Plusarg type mismatch - " << p << " is defined as " << info.type
                  << " (" << defined_size << " bytes) but accessed as " << sizeof(TYPE) << " bytes" << std::endl;
        assert(false);
    }
    if (sizeof(TYPE) < defined_size) {
        static std::set<std::string> reported;
        if (reported.insert(p).second) {
            std::cerr << "Warning: Plusarg " << p << " is defined as " << info.type
                      << " (" << defined_size << " bytes) but accessed as " << sizeof(TYPE) << " bytes; value truncated" << std::endl;
        }
    }
    return *((TYPE *)info.flag_ptr);
}


extern "C" {

    std::uint8_t cvm_plusargs_get_bool(const char* p) {
        return get<bool>(p);
    }

    std::int32_t cvm_plusargs_get_int32(const char* p) {
        return get<std::int32_t>(p);
    }

    std::int64_t cvm_plusargs_get_int64(const char* p) {
        return get<std::int64_t>(p);
    }

    std::uint64_t cvm_plusargs_get_uint64(const char* p) {
        return get<std::uint64_t>(p);
    }

    double cvm_plusargs_get_double(const char* p) {
        return get<double>(p);
    }

    const char* cvm_plusargs_get_string(const char* p) {
        static std::string s;
        s = get<std::string>(p);
        return s.c_str();
    }

    // New function that returns a fixed-size char array
    void cvm_plusargs_get_string_bytes_1024(const char* p, unsigned char str_buffer[1024]) {
        
        const char* str = cvm_plusargs_get_string(p);
        size_t len = strlen(str);

        if (len > 1023) {
            std::cerr << "ERROR: +" << p << "=" << str << " is too long\n";
            assert(false);
        }

        memcpy(str_buffer, str, len);
        str_buffer[len] = '\0';
    }
}
