// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "logger.hpp"
#include "cvm/plusargs.hpp"
#include <unordered_map>
#include <filesystem>
#include <limits>
#include <iostream>
#include <fstream>

static const std::unordered_map<std::string,cvm::verbosity_level> levels{
    {"NONE"  , cvm::NONE  },
    {"LOW"   , cvm::LOW   },
    {"MEDIUM", cvm::MEDIUM},
    {"HIGH"  , cvm::HIGH  },
    {"FULL"  , cvm::FULL  },
    {"DEBUG" , cvm::DEBUG },
};

static bool validate_verbosity(const char* flagname, const std::string& value) {
    auto it = levels.find(value);
    if (it == levels.end()) {
        cvm::log(cvm::NONE, "Invalid value for +{}={}\n", flagname, value);
        return false;
    }

    cvm::set_verbosity(it->second);

    return true;
}

DEFINE_string(cvm_verbosity, "MEDIUM", "cvm logging verbosity, valid values match uvm verbosity levels");
DEFINE_validator(cvm_verbosity, &validate_verbosity);
DEFINE_uint64(cvm_max_log_size, std::numeric_limits<std::uint64_t>::max(), "Maximum size of the log file in bytes");

cvm::verbosity_level cvm::logger::verbosity = cvm::verbosity_level::MEDIUM;
decltype(cvm::logger::handlers)  cvm::logger::handlers{};
decltype(cvm::logger::prefix  )  cvm::logger::prefix = [] () { return ""; };
decltype(cvm::logger::ostream )& cvm::logger::ostream = std::cout;

cvm::file_logger::~file_logger() = default;

cvm::file_logger::file_logger(const std::string& filename) : filename(filename) {}

void cvm::file_logger::check_and_rotate() {
    if (!output_file || !output_file->is_open()) {
        output_file = std::make_unique<std::ofstream>(filename, std::ios::out);
    } else if (std::uint64_t(output_file->tellp()) >= FLAGS_cvm_max_log_size) {
        rotate_log();
    }
}

void cvm::file_logger::rotate_log() {
  output_file->close();  // Close the current file

  // Rename the current log file by appending the .old suffix
  std::string old_filename = filename + ".old";
  std::filesystem::rename(filename, old_filename);  // Overwrite old file if it exists

  // Reopen a new log file
  output_file = std::make_unique<std::ofstream>(filename, std::ios::out);
}

void cvm::file_logger::flush() {
    if (output_file->is_open()) {
        output_file->flush();
    }
}

extern "C" {
    uint32_t cvm_logger_get_verbosity(const char* v) {
        return levels.at(std::string(v));
    }
    void cvm_logger_set_verbosity(uint32_t v) {
        cvm::set_verbosity(static_cast<cvm::verbosity_level>(v));
    }

    uint32_t cvm_logger_get_verbosity_from_plusargs(const char* p) {
        const char* v = cvm_plusargs_get_string(p);
        if (!v) {
            // Error out if the plusarg is not set
            cvm::log(cvm::ERROR, "ERROR: +{}={} is not set\n", p, v);
        }
        return levels.at(std::string(v));
    }
}
