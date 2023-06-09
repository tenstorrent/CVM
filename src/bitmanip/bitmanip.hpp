#pragma once

#include <cinttypes>
#include <algorithm>
#include <cassert>
#include <bitset>
#include <vector>

namespace cvm {

    struct bitmanip {

        template <typename T>
        struct is_bitset : std::false_type {};

        template <std::size_t N>
        struct is_bitset<std::bitset<N>> : std::true_type {};

        template <typename T>
        inline static constexpr bool is_bitset_v = is_bitset<T>::value;

        template <typename T>
            static constexpr T mask(size_t bits) {
                size_t size;
                if constexpr (is_bitset_v<T>)
                    size = T().size();
                else
                    size = 8*sizeof(T);

                assert(size >= bits);
                if (size == bits) return ~T(0);
                return ~(~T(0) << bits);
            }

        template <typename T>
            static constexpr T mask(size_t msb, size_t lsb) {
                if constexpr (is_bitset_v<T>)
                    assert(T().size() > msb);
                else
                    assert(8*sizeof(T) > msb);
                return mask<T>(msb - lsb + 1) << lsb;
            }

        template <typename T>
            static constexpr T slice(const T& t, size_t msb, size_t lsb) {
                return (t & mask<T>(msb, lsb)) >> lsb;
            }

        template <typename T, typename U = std::vector<uint8_t>, typename std::enable_if<std::is_fundamental_v<typename U::value_type> &&
                                                                                !std::is_same<U, std::vector<bool>>{}, bool>::type = true>
            static constexpr U slice(const T& t) {
                U u{};
                auto el_size = 8*sizeof(typename U::value_type);
                if constexpr (is_bitset_v<T>) {
                    for (size_t i = 0; (i + el_size) <= t.size(); i += el_size) {
                        auto msb = i + el_size - 1;
                        auto lsb = i;
                        auto res = slice(t, msb, lsb);
                        u.push_back(res.to_ulong());
                    }
                }
                else {
                    for (size_t i = 0; (i + el_size) <= 8*sizeof(t); i += el_size) {
                        auto msb = i + el_size - 1;
                        auto lsb = i;
                        auto res = slice(t, msb, lsb);
                        u.push_back(res);
                    }
                }
                return u;
            }

        template <typename T, typename U = std::vector<bool>, typename std::enable_if<std::is_fundamental<typename U::value_type>{} &&
                                                                              std::is_same<U, std::vector<bool>>{}, bool>::type = true>
            static constexpr U slice(const T& t) {
                U u{};
                if constexpr (is_bitset_v<T>) {
                    for (size_t i = 0; i < t.size(); i++) {
                        auto msb = i, lsb = i;
                        auto res = slice(t, msb, lsb);
                        u.push_back(res.to_ulong());
                    }
                }
                else {
                    for (size_t i = 0; i < 8*sizeof(t); i++) {
                        auto msb = i, lsb = i;
                        auto res = slice(t, msb, lsb);
                        u.push_back(res);
                    }
                }
                return u;
            }

        template <typename V, typename T>
            static constexpr V array_slice(const T* arr, const size_t msb, const size_t lsb) {
                const std::size_t G = 8*sizeof(T);

                V v(0);

                size_t bits_left_in_g = 0;
                for (size_t bit = lsb; bit <= msb; bit += bits_left_in_g) {

                    size_t bits_left = msb - bit + 1;
                    size_t lsb_g = bit % G;
                    bits_left_in_g = G - lsb_g;
                    size_t bits_to_take_in_g = std::min(bits_left, bits_left_in_g);
                    size_t msb_g = bits_to_take_in_g + lsb_g - 1;

                    v |= V(slice(arr[bit / G], msb_g, lsb_g)) << (bit - lsb);
                }

                return v;
            };

    };
}
