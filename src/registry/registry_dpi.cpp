#include "svdpi.h"
#include "cvm/callbacks.hpp"
#include "cvm/registry.hpp"
#include "cvm/topology.hpp"

extern "C" {

  void cvm_set_scope(cvm::topology::loc_t location, const char* s) {
    svScope scope = svGetScopeFromName(s);

    if (scope == nullptr) {
      cvm::log(cvm::ERROR, "cvm_set_scope: could not resolve scope for '{}' (loc {})\n", s, location);
      return;
    }
    cvm::registry::callbacks.set_scope(location, scope);
  }

}
