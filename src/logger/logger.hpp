// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <fmt/ostream.h>
#include <unordered_map>
#include <functional>
#include <string_view>
#include <memory>
#include <iosfwd>
#include <type_traits>
#include <utility>

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

            // Map enums to their underlying integer *before* they reach fmt, so fmt 9.x's
            // deprecated implicit enum->underlying path (arg_mapper::map, fmt/core.h:1470,
            // marked FMT_DEPRECATED) is never instantiated. Non-enums are perfect-forwarded
            // untouched. fmt::underlying is fmt's own non-deprecated replacement; format
            // specs like {:016x} still work since the value is an integer.
            template <typename T>
            static constexpr decltype(auto) log_arg(T&& v) {
                if constexpr (std::is_enum_v<std::remove_cvref_t<T>>)
                    return fmt::underlying(v);
                else
                    return std::forward<T>(v);
            }

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
                        fmt::print(out, fmt::runtime(format), log_arg(std::forward<Args>(args))...);

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
