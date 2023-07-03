#include "logger.hpp"
#include "cvm/plusargs.hpp"
#include <unordered_map>

static bool validate_verbosity(const char* flagname, const std::string& value) {
    std::unordered_map<std::string,cvm::verbosity_level> levels{
        {"NONE"  , cvm::NONE  },
        {"LOW"   , cvm::LOW   },
        {"MEDIUM", cvm::MEDIUM},
        {"HIGH"  , cvm::HIGH  },
        {"FULL"  , cvm::FULL  },
        {"DEBUG" , cvm::DEBUG },
    };

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

cvm::verbosity_level cvm::logger::verbosity = cvm::verbosity_level::MEDIUM;
std::unordered_map<cvm::verbosity_level, std::vector<std::function<void()>>> cvm::logger::handlers = {};

void cvm::file_logger::flush() {
    output_file.flush();
}
