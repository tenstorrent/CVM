#include "cvm/random.hpp"
#include <gflags/gflags.h>
#include <iostream>
#include <cassert>

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
}

uint32_t cvm::rand::get(std::string flag) {
    auto [min, max] = parse(flag);
    std::uniform_int_distribution<uint32_t> distrib(min, max);
    return distrib(gen);
}

std::pair<uint32_t,uint32_t> cvm::rand::parse(std::string flag) {
    size_t pos = flag.find(':');

    try {
        if (pos == std::string::npos) {
            uint32_t val = std::stoul(flag);
	    return {val, val};
        } else {
            uint32_t min = std::stoul(flag.substr(0, pos));
            uint32_t max = std::stoul(flag.substr(pos + 1));
            if (min > max) {
                std::cout << "Error: Invalid rand flag values\n";
                std::cout << "flag: " << flag << ", min must be less than or equal to max" << flag << std::endl;
                assert(0 && "Invalid plusarg");
            }
            return {min, max};
        }
    }
    catch (const std::exception& e) {
        std::cout << "Error: Failed to parse rand flag\n";
        std::cout << "flag: " << flag << ", exception: " << e.what() << std::endl;
        assert(0 && "Invalid plusarg");
    }

    return {0,0};
}

extern "C" {

    std::uint32_t cvm_rand_get(const char* p) {
        static std::string s;
        s = get_flag<std::string>(p);
        return cvm::rand::get(s);
    }

}
