// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <cinttypes>
#include <gflags/gflags.h>
#include <cassert>
#include <iostream>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

template <typename TYPE>
static constexpr std::string_view type_name() {
    if constexpr (std::is_same_v<TYPE, bool>)                          return "bool";
    else if constexpr (std::is_same_v<TYPE, GFLAGS_NAMESPACE::int32>)  return "int32";
    else if constexpr (std::is_same_v<TYPE, GFLAGS_NAMESPACE::uint32>) return "uint32";
    else if constexpr (std::is_same_v<TYPE, GFLAGS_NAMESPACE::int64>)  return "int64";
    else if constexpr (std::is_same_v<TYPE, GFLAGS_NAMESPACE::uint64>) return "uint64";
    else if constexpr (std::is_same_v<TYPE, double>)                   return "double";
    else if constexpr (std::is_same_v<TYPE, std::string>)              return "string";
    else static_assert(sizeof(TYPE) == 0, "type has no gflags storage");
}

template <typename TYPE>
static TYPE read(const char* p, const gflags::CommandLineFlagInfo& flag) {
    if (type_name<TYPE>() != flag.type) {
        std::cerr << "Error: Plusarg type mismatch - " << p << " is defined as " << flag.type
                  << " but accessed as " << type_name<TYPE>() << std::endl;
        assert(false);
        return TYPE{};
    }
    return *static_cast<const TYPE*>(flag.flag_ptr);
}

template <typename TYPE>
TYPE get(const char* p) {

    gflags::CommandLineFlagInfo info;
    bool found = gflags::GetCommandLineFlagInfo(p, &info);
    if (!found) {
        std::cerr << "Error: Plusarg not found - " << p << std::endl;
        assert(false);  // Force assertion failure after printing
    }

    return read<TYPE>(p, info);
}


extern "C" {

    std::uint8_t cvm_plusargs_get_bool(const char* p) {
        return get<bool>(p);
    }

    std::int32_t cvm_plusargs_get_int32(const char* p) {
        return get<std::int32_t>(p);
    }

    std::uint32_t cvm_plusargs_get_uint32(const char* p) {
        return get<std::uint32_t>(p);
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
        s = get<decltype(s)>(p);
        return s.c_str();
    }

    void cvm_plusargs_get_string_bytes_1024(const char* p, unsigned char str_buffer[1024]) {
        std::string str = get<std::string>(p);
        const std::size_t max_len = 1024 - 1;
        const bool fits = str.size() <= max_len;

        if (!fits) {
            std::cerr << "ERROR: +" << p << "=" << str << " is too long\n";
            assert(false);
        }
        const std::size_t n = fits ? str.size() : max_len;
        std::memcpy(str_buffer, str.data(), n);
        std::memset(str_buffer + n, 0, 1024 - n);
    }
}
