#pragma once
#include <string_view>

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
          static constexpr std::string_view name() {
              constexpr auto wrapped_name = wrapped_type_name<T>();
              constexpr auto prefix_length = wrapped_type_name_prefix_length();
              constexpr auto suffix_length = wrapped_type_name_suffix_length();
              constexpr auto type_name_length = wrapped_name.length() - prefix_length - suffix_length;
              return wrapped_name.substr(prefix_length, type_name_length);
          }
    };
}
