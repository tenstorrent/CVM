#include <gtest/gtest.h>
#include "cvm/logger.hpp" 
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>

void check(const std::string& filename) {
    std::ifstream t(filename);
    std::string testlog((std::istreambuf_iterator<char>(t)),
            std::istreambuf_iterator<char>());
    std::string expectedOutput = R"xxx(medium 2
high 3
)xxx";

    ASSERT_EQ(expectedOutput,testlog);
}

TEST(Logger, StdoutLogger) {
    int pfd = open("stdout.log", O_WRONLY | O_CREAT, 0777);
    int saved = dup(1);

    close(1);
    dup(pfd);
    close(pfd);

    cvm::set_verbosity(cvm::MEDIUM);
    cvm::log(cvm::HIGH, "high {}\n", 1);
    cvm::log(cvm::MEDIUM, "medium {}\n", 2);
    cvm::set_verbosity(cvm::HIGH);
    cvm::log(cvm::HIGH, "high {}\n", 3);
    fflush(stdout);

    // restore it back
    dup2(saved, 1);
    close(saved);

    check("stdout.log");
}

TEST(Logger, FileLogger) {

    {
        cvm::file_logger log("test.log");
        cvm::set_verbosity(cvm::MEDIUM);
        log(cvm::HIGH, "high {}\n", 1);
        log(cvm::MEDIUM, "medium {}\n", 2);
        cvm::set_verbosity(cvm::HIGH);
        log(cvm::HIGH, "high {}\n", 3);
    }


    check("test.log");
}
