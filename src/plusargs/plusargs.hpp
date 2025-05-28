#include <gflags/gflags.h>


namespace cvm {

    namespace plusargs {

        void parse();

        

    }
}

// C interface for plusargs access from external code
extern "C" {
    const char* cvm_plusargs_get_string(const char* p);
}
