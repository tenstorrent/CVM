#include <cinttypes>
#include <gflags/gflags.h>
#include <cassert>
#include <iostream>

template <typename TYPE>
TYPE get(const char* p) {

    gflags::CommandLineFlagInfo info;
    bool found = gflags::GetCommandLineFlagInfo(p, &info);
    if (!found) {
        std::cerr << "Error: Plusarg not found - " << p << std::endl;
        assert(false);  // Force assertion failure after printing
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
}
