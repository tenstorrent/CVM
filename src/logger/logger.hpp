#pragma once
#include <fmt/os.h>
#include <unordered_map>
#include <functional>
#include <string_view>
#include <memory>

namespace cvm {

    enum verbosity_level {
        ERROR  =   0,
        NONE   =   1,
        LOW    = 100,
        MEDIUM = 200,
        HIGH   = 300,
        FULL   = 400,
        DEBUG  = 500,
    };


    class logger {

        private:

            static verbosity_level verbosity;
            static std::unordered_map<verbosity_level, std::function<void()>> handlers;
            static std::function<std::string_view()> prefix;

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


            template <typename... Args>
                logger(verbosity_level v, Args&&... args) {

                    if (check_verbosity(v)) {

                        fmt::print(prefix());
                        fmt::print(std::forward<Args>(args)...);

                    }

                    auto it = handlers.find(v);
                    if (it != handlers.end()) {

                        (it->second)();

                    }

                }

            template <typename... Args>
                logger(fmt::ostream& out, verbosity_level v, Args&&... args) {

                    if (check_verbosity(v)) {

                        out.print(prefix());
                        out.print(std::forward<Args>(args)...);

                    }

                    auto it = handlers.find(v);
                    if (it != handlers.end()) {

                        (it->second)();

                    }

                }
    };

    class file_logger {

        private:

            const std::string filename;
            std::unique_ptr<fmt::ostream> output_file;

        public:

            file_logger(const std::string& filename) :
                filename(filename) {}

            template <typename... Args>
                void log(verbosity_level v, Args&&... args) {

                    if (logger::check_verbosity(v) && !output_file)
                        output_file = std::make_unique<fmt::ostream>(fmt::output_file(filename));

                    logger(*output_file, v, std::forward<Args>(args)...);

                }

            template <typename... Args>
                auto operator()(Args&&... args) -> decltype(log(std::forward<Args>(args)...)) {
                    return log(std::forward<Args>(args)...);
                }

            void flush();

    };

    struct log {

        template <typename... Args>
            log(Args&&... args) {
                logger(std::forward<Args>(args)...);
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
