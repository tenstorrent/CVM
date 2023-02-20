#include "cvm/registry.hpp"

extern "C" {
    void cvm_registry_reset() {
        return cvm::registry::reset();
    }
}
