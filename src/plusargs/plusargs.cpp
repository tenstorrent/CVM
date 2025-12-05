#include "cvm/plusargs.hpp"
#include <algorithm>
#include <vector>
#include <string>
#include <string_view>
#include <regex>
#include "vpi_user.h"
#include <memory>
#include <any>
#include <ranges>
#include <functional>

void cvm::plusargs::parse() {

    using namespace std::literals;

    // this will reinitialize flags between tests
    // otherwise, flags that are not explicitly specified by a later test will retain the value from a previous test
    static std::unique_ptr<gflags::FlagSaver> flags_saver;
    flags_saver.reset();
    flags_saver = std::make_unique<gflags::FlagSaver>();

    s_vpi_vlog_info info;
    vpi_get_vlog_info(&info);

    std::string_view undefok_regexp = "+undefok_regexp="sv;
    std::vector<std::string> argvv;

    auto argv_view = std::ranges::views::counted(info.argv, info.argc);

    auto starts_with_undefok_regexp = [undefok_regexp](const char* arg) { return std::string_view(arg).starts_with(undefok_regexp); };

    auto regex_view = argv_view
                      | std::ranges::views::filter(starts_with_undefok_regexp)
                      | std::ranges::views::transform([undefok_regexp](const char* arg) {
                          return std::string_view(arg).substr(undefok_regexp.length()).data();
                      });

    for (const char* argv_item : argv_view | std::ranges::views::filter(std::not_fn(starts_with_undefok_regexp))) {
        if (argv_item[0] == '+') {
            if (std::ranges::any_of(regex_view, [argv_item](const char* pattern) {
                                                    return std::regex_search(argv_item, std::regex(pattern));
                                                }))
                continue;

            auto arg = std::string("--") + std::string(argv_item + 1);
            if (!std::regex_search(arg, std::regex(R"(\+\d+)"))) {
                std::replace(arg.begin(), arg.end(), '+', '_');
            }
            argvv.push_back(std::move(arg));
        } else {
            argvv.push_back(argv_item);
        }
    }

    struct args {
        int argc = 0;
        char** argv = nullptr;

        args(std::vector<std::string>& argvv) {
            argc = argvv.size();
            argv = new char*[argc];
            for (int i = 0; i < argc; i++) {
                argv[i] = argvv[i].data();
            }
        }

        ~args() {
            delete[] argv;
        }

    };

    args args(argvv);

    //gflags::AllowCommandLineReparsing();
    gflags::ParseCommandLineFlags(&args.argc, &args.argv, false);

}
