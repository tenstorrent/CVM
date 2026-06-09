// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include "cvm/logger.hpp"
#include "cvm/plusargs.hpp"
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>
#include <filesystem>

#include "logger_mock.hpp"

#include "vpi_user.h"

namespace {
	int argc;
	std::vector<std::string> argv;

  void set_argc_argv(int argc, const char** argv) {
    ::argc = argc;
    ::argv.clear();
    for (int i = 0; i < argc; i++) {
      ::argv.emplace_back(argv[i]);
    }
  }

}

PLI_INT32 vpi_get_vlog_info(p_vpi_vlog_info vlog_info_p) {
  static const char* argv[20];
  assert(argc <= 20);
  vlog_info_p->argc = ::argc;
  for (int i = 0; i < ::argc; i++) {
    argv[i] = ::argv[i].c_str();
  }
  vlog_info_p->argv = (PLI_BYTE8**) argv;
  return 1;
}

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Remove test files if they exist from previous tests
        std::filesystem::remove("stdout.log");
        std::filesystem::remove("test.log");
        std::filesystem::remove("test.log.old");

				const char* argv[] = {
					"./logger_test",
				};

				std::cerr << "Parsing plusargs" << std::endl;
        set_argc_argv(sizeof(argv)/sizeof(argv[0]), argv);
				cvm::plusargs::parse();
    }

    void TearDown() override {
        // Clean up test files after each test
        std::filesystem::remove("stdout.log");
        std::filesystem::remove("test.log");
        std::filesystem::remove("test.log.old");
    }
};

void check(const std::string& filename) {
    std::ifstream t(filename);
    std::string testlog((std::istreambuf_iterator<char>(t)),
            std::istreambuf_iterator<char>());
    std::string expectedOutput = R"xxx(b 2
c 3
)xxx";

    ASSERT_EQ(expectedOutput,testlog);
}

TEST_F(LoggerTest, StdoutLogger) {

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
    cvm::set_verbosity(cvm::MEDIUM);
    fflush(stdout);

    // restore it back
    dup2(saved, 1);
    close(saved);

    check("stdout.log");
}

void file_logger_basic_test() {

    {
        cvm::file_logger log("test.log");
        cvm::set_verbosity(cvm::MEDIUM);
        log(cvm::HIGH, "a {}\n", 1);
        log(cvm::MEDIUM, "b {}\n", 2);
        cvm::set_verbosity(cvm::HIGH);
        log(cvm::HIGH, "c {}\n", 3);
        cvm::set_verbosity(cvm::MEDIUM);
    }


    check("test.log");

}

TEST_F(LoggerTest, FileLogger) {

      file_logger_basic_test();

}

TEST_F(LoggerTest, Plusargs) {
    const char* argv[] = {
        "./logger_test",
        "+cvm_verbosity=LOW"
    };

    set_argc_argv(sizeof(argv)/sizeof(argv[0]), argv);
    cvm::plusargs::parse();

		ASSERT_FALSE(cvm::logger::check_verbosity(cvm::MEDIUM));

    {
        cvm::file_logger log("test.log");
        log(cvm::MEDIUM, "a {}\n", 1);
        log(cvm::LOW, "b {}\n", 2);
        log(cvm::LOW, "c {}\n", 3);
    }


    check("test.log");

}

TEST_F(LoggerTest, Handler) {
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

    cvm::set_logger_prefix([]() -> std::string_view { return ""; });
}

TEST_F(LoggerTest, Exists) {
    cvm::file_logger log("test.log");
    ASSERT_EQ(access("test.log", F_OK), -1);
    cvm::set_verbosity(cvm::MEDIUM);
    log(cvm::HIGH, "hello");
    ASSERT_EQ(access("test.log", F_OK), -1);
    cvm::set_verbosity(cvm::HIGH);
    log(cvm::HIGH, "hello");
    ASSERT_EQ(access("test.log", F_OK), 0);
}

TEST_F(LoggerTest, NoRotationIfUnderMaxSize) {
    const char* argv[] = {
        "./logger_test",
        "+cvm_max_log_size=1024"
    };

    set_argc_argv(sizeof(argv)/sizeof(argv[0]), argv);
    cvm::plusargs::parse();

    file_logger_basic_test();

    ASSERT_FALSE(std::filesystem::exists("test.log.old"));
}

TEST_F(LoggerTest, RotationOccursWhenMaxSizeExceeded) {
    const char* argv[] = {
        "./logger_test",
        "+cvm_max_log_size=1024"  // 1 KB max size
    };

    set_argc_argv(sizeof(argv)/sizeof(argv[0]), argv);
    cvm::plusargs::parse();
    cvm::file_logger logger("test.log");

    int i = 0;
    int digits = 0;
    int next_digit = 1;
    int chars = 0;

    for (int overwrites = 0; overwrites < 3; overwrites++) {
        int num = 0;
        for (int start_chars = chars, prev_chars = chars; chars < start_chars + 1024 + 512; i++) {
            if ((prev_chars/1024) != (chars/1024)) {
                num = i-1;
            }
            logger(cvm::MEDIUM, "{}\n", i);
            if (!(i % next_digit)) {
                digits += 1;
                next_digit = 10 * next_digit;
            }
            prev_chars = chars;
            chars += digits + 1;
        }
        logger.flush();


        std::ifstream new_log_file("test.log");
        std::string new_log_content((std::istreambuf_iterator<char>(new_log_file)),
            std::istreambuf_iterator<char>());
        EXPECT_EQ(std::to_string(num+1), new_log_content.substr(0, new_log_content.find("\n")));

        ASSERT_TRUE(std::filesystem::exists("test.log.old"));
        std::ifstream old_log_file("test.log.old");
        std::string old_log_content((std::istreambuf_iterator<char>(old_log_file)),
            std::istreambuf_iterator<char>());
        EXPECT_EQ(std::to_string(num) + "\n", old_log_content.substr(old_log_content.rfind("\n", old_log_content.size() - 2) + 1));
    }
}
