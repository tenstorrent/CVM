// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "svdpi.h"
#include "cvm/callbacks.hpp"
#include "cvm/registry.hpp"
#include "cvm/topology.hpp"

extern "C" {

  int cvm_registry_set_scope(unsigned int location) {
    if (location == cvm::topology::null) {
      cvm::log(cvm::ERROR, "cvm_registry_set_scope: null location  for callback\n");
      return 0;
    }
    svScope scope = svGetScope();

    if (scope == nullptr) {
      cvm::log(cvm::ERROR, "cvm_registry_set_scope: null svScope for loc {}\n", location);
      return 0;
    }
    cvm::registry::callbacks.set_scope(location, scope);
    return 0;
  }

}
