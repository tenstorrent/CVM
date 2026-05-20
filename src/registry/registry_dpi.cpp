#include "svdpi.h"
#include "cvm/callbacks.hpp"
#include "cvm/registry.hpp"
#include "cvm/topology.hpp"

extern "C" {

  void cvm_set_scope(unsigned int location) {
    svScope s = svGetScope();
    cvm::topology::loc_t loc = location;
    cvm::registry::callbacks.set_scope(loc, s);
  }

}
