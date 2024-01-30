#include <random>
#include <limits>

namespace cvm {

    template<typename T, T LO = std::numeric_limits<T>::min(), T HI = std::numeric_limits<T>::max()>
    struct rng {
        static_assert(std::is_integral_v<T>, "T must be an integral type for uniform_int_distribution");
        std::mt19937 gen;
        std::uniform_int_distribution<T> distrib;
        rng(uint64_t seed) : gen(seed), distrib(LO, HI) {}
        T operator()() {
            return distrib(gen);
        }
    };

}
