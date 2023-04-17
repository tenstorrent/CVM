#pragma once
#include <fmt/os.h>
#include <unordered_map>
#include <functional>

namespace cvm {

    enum verbosity_level {
        NONE   =   0,
        ERROR  =   0,
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

        public:

            static void set_verbosity(verbosity_level v) {

                verbosity = v;

            }


            static void set_handler(verbosity_level v, std::function<void()> handler) {

                handlers[v] = handler;

            }


            template <typename... Args>
                logger(verbosity_level v, Args&&... args) {

                    if (v <= verbosity) {

                        fmt::print(std::forward<Args>(args)...);
                        auto it = handlers.find(v);
                        if (it != handlers.end()) {

                          (it->second)();

                        }

                    }

                }

            template <typename... Args>
                logger(fmt::ostream& out, verbosity_level v, Args&&... args) {

                    if (v <= verbosity) {

                        out.print(std::forward<Args>(args)...);
                        auto it = handlers.find(v);
                        if (it != handlers.end()) {

                          (it->second)();

                        }

                    }

                }
    };

    class file_logger {

        private:

            fmt::ostream output_file;

        public:

            file_logger(const std::string& filename) :
                output_file(fmt::output_file(filename)) {}

            template <typename... Args>
                void log(Args&&... args) {

                    logger(output_file, std::forward<Args>(args)...);

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
        void set_handler(Args&&... args) {
            logger::set_handler(std::forward<Args>(args)...);
        }
}
