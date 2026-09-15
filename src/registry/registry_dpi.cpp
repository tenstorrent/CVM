// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "svdpi.h"
#include "cvm/callbacks.hpp"
#include "cvm/registry.hpp"
#include "cvm/topology.hpp"

extern "C" {

  int cvm_registry_set_scope(unsigned int location) {
    if (location == cvm::topology::null) {
      cvm::log(cvm::ERROR, "Error: cvm_registry_set_scope: null location  for callback\n");
      return 0;
    }
    svScope scope = svGetScope();

    if (scope == nullptr) {
      cvm::log(cvm::ERROR, "Error: cvm_registry_set_scope: null svScope for loc {}\n", location);
      return 0;
    }
    cvm::registry::callbacks.set_scope(location, scope);
    return 0;
  }

  // Driven from the design's clock by cvm_registry_callbacks, so nothing
  // host-side has to own a loop. Returns a value only to keep a simulator from
  // reordering it.
  unsigned char cvm_registry_flush_callbacks() {
    cvm::registry::callbacks.flush();
    return 1;
  }

}
