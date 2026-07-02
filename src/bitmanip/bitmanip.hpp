// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cinttypes>
#include <algorithm>
#include <cassert>
#include <vector>
#include "cvm/type_traits.hpp"

namespace cvm {

    struct bitmanip {

        template <typename T>
            static constexpr size_t width() {
                if constexpr (type_traits::is_bitset_v<T>)
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

        template <size_t I, typename T>
            static constexpr bool index(const T& t) {
                static_assert(I < width<T>());
                if constexpr (type_traits::is_bitset_v<T>) {
                    return t[I];
                } else {
                    return (t >> I) & 1;
                }
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
                if constexpr (type_traits::is_bitset_v<T>) {
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
                if constexpr (type_traits::is_bitset_v<T>) {
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

        template <typename V, std::size_t ELEM_SIZE = 0, typename T>
            static constexpr V array_slice(const T* arr, const size_t msb, const size_t lsb) {
                const std::size_t G = 8*sizeof(T);

                auto v = []() -> auto {
                  if constexpr (type_traits::is_array_v<V>)
                    return V{};
                  else
                    return V(0);
                }();

                for (size_t bit = lsb; bit <= msb;) {

                    size_t bits_left = msb - bit + 1;
                    size_t lsb_g = bit % G;
                    size_t bits_left_in_g = G - lsb_g;

                    if constexpr (type_traits::is_array_v<V>) {
                      using ET = type_traits::remove_all_array_extents<V>::type;
                      static_assert(ELEM_SIZE > 0);
                      static_assert(8*sizeof(ET) >= ELEM_SIZE);

                      // Overlay elements of bit width ELEM_SIZE into result.
                      // This can be used as a scatter operation.
                      size_t bits_taken = bit - lsb;
                      size_t bits_left_in_n = ELEM_SIZE - (bits_taken % ELEM_SIZE);
                      size_t bits_to_take_in_g = std::min(std::min(bits_left, bits_left_in_g), bits_left_in_n);
                      size_t msb_g = bits_to_take_in_g + lsb_g - 1;

                      size_t index = (bit - lsb) / ELEM_SIZE;
                      *(type_traits::get_array_base_ptr(v) + index) |= ET(slice(arr[bit / G], msb_g, lsb_g)) << ((bit - lsb) % ELEM_SIZE);
                      bit += bits_to_take_in_g;
                    }
                    else {
                      size_t bits_to_take_in_g = std::min(bits_left, bits_left_in_g);
                      size_t msb_g = bits_to_take_in_g + lsb_g - 1;
                      v |= V(slice(arr[bit / G], msb_g, lsb_g)) << (bit - lsb);
                      bit += bits_left_in_g;
                    }
                }

                return v;
            };

    };
}
