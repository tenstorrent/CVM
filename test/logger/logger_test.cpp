#include <gtest/gtest.h>
#include "cvm/logger.hpp"
#include "cvm/plusargs.hpp"
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>

#include "Vlogger_test.h"

#include "logger_mock.hpp"

void check(const std::string& filename) {
    std::ifstream t(filename);
    std::string testlog((std::istreambuf_iterator<char>(t)),
            std::istreambuf_iterator<char>());
    std::string expectedOutput = R"xxx(b 2
c 3
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
    cvm::log(cvm::HIGH, "a {}\n", 1);
    cvm::log(cvm::MEDIUM, "b {}\n", 2);
    cvm::set_verbosity(cvm::HIGH);
    cvm::log(cvm::HIGH, "c {}\n", 3);
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
        log(cvm::HIGH, "a {}\n", 1);
        log(cvm::MEDIUM, "b {}\n", 2);
        cvm::set_verbosity(cvm::HIGH);
        log(cvm::HIGH, "c {}\n", 3);
    }


    check("test.log");
}

TEST(Logger, Plusargs) {
    const char* argv[] = {
        "./logger_test",
        "+cvm_verbosity=LOW"
    };

    Verilated::commandArgs(sizeof(argv)/sizeof(argv[0]), argv);
    cvm::plusargs::parse();

    {
        cvm::file_logger log("test.log");
        log(cvm::MEDIUM, "a {}\n", 1);
        log(cvm::LOW, "b {}\n", 2);
        log(cvm::LOW, "c {}\n", 3);
    }


    check("test.log");

}

TEST(Logger, Handler) {
    cvm::set_verbosity(cvm::MEDIUM);

    MockHandler handler;
    cvm::set_logger_handler(cvm::ERROR, [&handler]() { return handler.handle(); });
    EXPECT_CALL(handler, handle()).Times(1);

    MockHandler prefix;
    cvm::set_logger_prefix([]() -> std::string_view { return "b "; });

    int pfd = open("handler.log", O_WRONLY | O_CREAT, 0777);
    int saved = dup(1);

    close(1);
    dup(pfd);
    close(pfd);

    cvm::log(cvm::ERROR, "2\nc 3\n");

    fflush(stdout);

    // restore it back
    dup2(saved, 1);
    close(saved);

    check("handler.log");
}

TEST(Logger, Exists) {
    cvm::file_logger log("exists.log");
    ASSERT_EQ(access("exists.log", F_OK), -1);
    cvm::set_verbosity(cvm::MEDIUM);
    log(cvm::HIGH, "hello");
    ASSERT_EQ(access("exists.log", F_OK), -1);
    cvm::set_verbosity(cvm::HIGH);
    log(cvm::HIGH, "hello");
    ASSERT_EQ(access("exists.log", F_OK), 0);
}
