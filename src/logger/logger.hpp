#pragma once
#include <fmt/ostream.h>
#include <unordered_map>
#include <functional>
#include <string_view>
#include <memory>
#include <iosfwd>
#include <type_traits>

// Render any enum as its underlying integer value. fmt 9.x deprecated the implicit
// enum->underlying mapping in arg_mapper (fmt/core.h:1470, marked FMT_DEPRECATED).
// That overload is guarded by !has_formatter<T>, so providing this formatter
// SFINAE-disables the deprecated path and routes every enum through here instead,
// retiring the whole class of -Wdeprecated-declarations errors. Inheriting from the
// underlying-integer formatter also preserves format specs (e.g. {:#x}).
template <typename E, typename Char>
struct fmt::formatter<E, Char, std::enable_if_t<std::is_enum_v<E>>>
    : fmt::formatter<std::underlying_type_t<E>, Char> {
  template <typename FormatContext>
  auto format(E value, FormatContext& ctx) const -> decltype(ctx.out()) {
    return fmt::formatter<std::underlying_type_t<E>, Char>::format(
        static_cast<std::underlying_type_t<E>>(value), ctx);
  }
};

namespace cvm {

    enum verbosity_level {
        ERROR  = 0,
        NONE   = 1,
        LOW    = 2,
        MEDIUM = 3,
        HIGH   = 4,
        FULL   = 5,
        DEBUG  = 6,
        NUM_VERBOSITY = 7,
    };


    class logger {

        private:

            static verbosity_level verbosity;
            static std::array<std::function<void()>, NUM_VERBOSITY> handlers;
            static std::function<std::string_view()> prefix;
            static std::ostream& ostream;

        public:

            static bool check_verbosity(verbosity_level v) {

                return v <= verbosity;

            }

            static void set_verbosity(verbosity_level v) {

                verbosity = v;

            }


            static void set_handler(verbosity_level v, const std::function<void()>& handler) {

                handlers[v] = handler;

            }


            static void set_prefix(const std::function<std::string_view()>& pre) {

                prefix = pre;

            }

            static void handle(verbosity_level v) {
                if (auto& f = handlers.at(v); f) {
                    f();
                }
            }

            // TODO: allow using consteval of format string in fmt

            template <typename... Args>
                static void log(verbosity_level v, std::string_view format, Args&&... args) {
                  log(ostream, v, format, std::forward<Args>(args)...);
                }

            template <typename... Args>
                static void log(std::ostream& out, verbosity_level v, std::string_view format, Args&&... args) {

                    if (check_verbosity(v)) {

                        fmt::print(out, "{}", prefix());
                        fmt::print(out, fmt::runtime(format), std::forward<Args>(args)...);

                    }

                    handle(v);
                }
    };

    class file_logger {

        private:

            const std::string filename;
            std::unique_ptr<std::ofstream> output_file;

            void rotate_log();
            void check_and_rotate();

        public:

            file_logger(const std::string& filename);

            template <typename... Args>
                void log(verbosity_level v, std::string_view format, Args&&... args) {

                    if (logger::check_verbosity(v)) {

                        check_and_rotate();

                        logger::log(*output_file, v, format, std::forward<Args>(args)...);
                    }

                }

            template <typename... Args>
                void operator()(verbosity_level v, std::string_view format, Args&&... args) {
                    log(v, format, std::forward<Args>(args)...);
                }

            void flush();

            ~file_logger();

    };

    struct log {

        template <typename... Args>
            log(verbosity_level v, std::string_view format, Args&&... args) {
                logger::log(v, format, std::forward<Args>(args)...);
            }

    };

    template <typename... Args>
        void set_verbosity(Args&&... args) {
            logger::set_verbosity(std::forward<Args>(args)...);
        }


    template <typename... Args>
        void set_logger_handler(Args&&... args) {
            logger::set_handler(std::forward<Args>(args)...);
        }


    template <typename... Args>
        void set_logger_prefix(Args&&... args) {
            logger::set_prefix(std::forward<Args>(args)...);
        }
}
