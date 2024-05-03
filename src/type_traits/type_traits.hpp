#pragma once
#include <string_view>
#include <bitset>

namespace cvm {
    // https://stackoverflow.com/questions/35941045/can-i-obtain-c-type-names-in-a-constexpr-way/35943472#35943472
    struct type_traits {
        private:
          using type_name_prober = void;

          template <typename T>
          static constexpr std::string_view wrapped_type_name() {
#if defined(__clang__) || defined(__GNUC__)
              return __PRETTY_FUNCTION__;
#else
#error "Unsupported compiler"
#endif
          }

          static constexpr std::size_t wrapped_type_name_prefix_length() {
              using namespace std::string_view_literals;
              return wrapped_type_name<type_name_prober>().find("void"sv);
          }

          static constexpr std::size_t wrapped_type_name_suffix_length() {
              using namespace std::string_view_literals;
              return wrapped_type_name<type_name_prober>().length()
                  - wrapped_type_name_prefix_length()
                  - "void"sv.length();
          }

        public:


          template <typename T>
          struct is_bitset : std::false_type {};

          template <std::size_t N>
          struct is_bitset<std::bitset<N>> : std::true_type {};

          template <typename T>
          inline static constexpr bool is_bitset_v = is_bitset<T>::value;

          template <typename T>
          struct is_array : std::false_type {};

          template <typename T, std::size_t N>
          struct is_array<std::array<T, N>> : std::true_type {};

          template <typename T>
          inline static constexpr bool is_array_v = is_array<T>::value;

          template <typename T>
          struct remove_all_array_extents {
            using type = T;
          };

          template <typename T, std::size_t N>
          struct remove_all_array_extents<std::array<T, N>> {
            using type = typename remove_all_array_extents<T>::type;
          };

          template <typename T>
          inline static constexpr remove_all_array_extents<T>::type* get_array_base_ptr(T& array) {
            if constexpr (is_array_v<T>)
              return get_array_base_ptr(*array.data());
            else
              return &array;
          };

          template <typename T>
          static constexpr std::string_view name() {
              constexpr auto wrapped_name = wrapped_type_name<T>();
              constexpr auto prefix_length = wrapped_type_name_prefix_length();
              constexpr auto suffix_length = wrapped_type_name_suffix_length();
              constexpr auto type_name_length = wrapped_name.length() - prefix_length - suffix_length;
              return wrapped_name.substr(prefix_length, type_name_length);
          }
    };
}
