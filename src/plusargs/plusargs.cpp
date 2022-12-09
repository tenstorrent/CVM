#include "cvm/plusargs.hpp"
#include <algorithm>
#include <vector>
#include <string>
#include "vpi_user.h"

void cvm::plusargs::parse() {

    s_vpi_vlog_info info;
    vpi_get_vlog_info(&info);
    
    std::vector<std::string> argvv;
    for (int i = 0; i < info.argc; i++) {
        if (info.argv[i][0] == '+') {
            argvv.push_back(std::string("--") + std::string(info.argv[i] + 1));
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
            delete argv;
        }

    };

    args args(argvv);

    gflags::AllowCommandLineReparsing();
    gflags::ParseCommandLineFlags(&args.argc, &args.argv, false);

}
