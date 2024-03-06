#include "cvm/plusargs.hpp"
#include <algorithm>
#include <vector>
#include <string>
#include <regex>
#include "vpi_user.h"
#include <memory>

void cvm::plusargs::parse() {
    // this will reinitialize flags between tests
    // otherwise, flags that are not explicitly specified by a later test will retain the value from a previous test
    static std::unique_ptr<gflags::FlagSaver> flags_saver;
    flags_saver = std::make_unique<gflags::FlagSaver>();

    s_vpi_vlog_info info;
    vpi_get_vlog_info(&info);
    
    std::vector<std::string> argvv;
    for (int i = 0; i < info.argc; i++) {
        if (info.argv[i][0] == '+') {
            std::string a = std::string("--") + std::string(info.argv[i] + 1);
            if (!std::regex_search(a, std::regex(R"(\+\d+)"))) {
                std::replace(a.begin(), a.end(), '+', '_');
            }
            argvv.push_back(a);
        } else {
            argvv.push_back(info.argv[i]);
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
