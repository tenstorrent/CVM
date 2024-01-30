#include <random>

namespace cvm {

    template<typename T, T LO, T HI>
    struct rng {
        static_assert(std::is_integral_v<T>, "T must be an integral type for uniform_int_distribution");
        std::mt19937 gen;
        std::uniform_int_distribution<> distrib = std::uniform_int_distribution<>(LO, HI);
        rng(uint64_t seed) : gen(seed) {}
        T operator()() {
            return distrib(gen);
        }
    };

}
