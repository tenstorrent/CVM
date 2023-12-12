#pragma once

#include <cinttypes>
#include <algorithm>
#include <cassert>
#include <bitset>
#include <vector>
#include <bit>
#include <type_traits>

namespace cvm {

    struct bitmanip {

        template <typename T>
        struct is_bitset : std::false_type {};

        template <std::size_t N>
        struct is_bitset<std::bitset<N>> : std::true_type {};

        template <typename T>
        inline static constexpr bool is_bitset_v = is_bitset<T>::value;

        template <typename T>
            static constexpr size_t width() {
                if constexpr (is_bitset_v<T>)
                    return T().size();
                else
                    return 8*sizeof(T);
            }

        template <typename T>
            static constexpr T mask(size_t bits) {
                size_t size = width<T>();
                assert(size >= bits);
                if (size == bits) return ~T(0);
                return ~(~T(0) << bits);
            }

        template <typename T>
            static constexpr T mask(size_t msb, size_t lsb) {
                size_t size = width<T>();
                assert(size > msb);
                return mask<T>(msb - lsb + 1) << lsb;
            }

        template <typename T>
            static constexpr T slice(const T& t, size_t msb, size_t lsb) {
                return (t & mask<T>(msb, lsb)) >> lsb;
            }

        template <typename T, typename U = std::vector<uint8_t>, typename std::enable_if<std::is_fundamental_v<typename U::value_type> &&
                                                                                !std::is_same_v<U, std::vector<bool>>, bool>::type = true>
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

        template <typename T, typename U = std::vector<bool>, typename std::enable_if<std::is_fundamental_v<typename U::value_type> &&
                                                                              std::is_same_v<U, std::vector<bool>>, bool>::type = true>
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

        template <typename V, size_t MSB, size_t LSB, size_t BIT, typename T>
            static constexpr void _array_slice(const T* arr, V& v) {

                constexpr size_t bits_left = MSB - BIT + 1;
                constexpr std::size_t G = std::max<size_t>(
                        8*sizeof(T),
                        std::endian::native != std::endian::little ? 0 :
                        bits_left >= 64 ? 64 :
                        bits_left >= 32 ? 32 :
                        bits_left >= 16 ? 16 :
                        0
                        );

                typedef typename std::conditional<G == 8*sizeof(T), T,
                        typename std::conditional<G == 64         , std::uint64_t,
                        typename std::conditional<G == 32         , std::uint32_t,
                        typename std::conditional<G == 16         , std::uint16_t,
                                 decltype(nullptr)
                                >::type>::type>::type>::type W;

                constexpr size_t lsb_g = BIT % G;
                constexpr size_t bits_left_in_g = G - lsb_g;
                constexpr size_t bits_to_take_in_g = std::min(bits_left, bits_left_in_g);
                constexpr size_t msb_g = bits_to_take_in_g + lsb_g - 1;
                constexpr size_t Gs = std::max(!lsb_g ? bits_left / G : size_t(0), size_t(1));

                for (size_t i = 0; i < Gs; i++) {
                    W s(slice(((W*)arr)[BIT / G + i], msb_g, lsb_g));

                    if constexpr(false && is_bitset_v<V>) {
                        for (size_t b = 0; b < bits_left_in_g; b++) {
                            v.set(BIT - LSB + i * G + b, (s >> b) & 1);
                        }
                    } else {
                        v |= V(s) << (BIT - LSB + i*G);
                    }
                }

                constexpr size_t next_bit = BIT + bits_left_in_g * Gs;
                if constexpr (next_bit <= MSB) {
                    _array_slice<V, MSB, LSB, next_bit, T>(arr, v);
                }
            };

        template <typename V, size_t MSB, size_t LSB, typename T>
            static constexpr V array_slice(const T* arr) {
                V v{0};
                _array_slice<V, MSB, LSB, LSB, T>(arr, v);
                return v;
            }
    };
}
