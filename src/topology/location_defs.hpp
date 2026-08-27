// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace cvm {

  namespace location_defs {

    typedef uint32_t loc_t;
    inline constexpr loc_t null = 0;

  }

  // Aliased into cvm::topology so translation units that only include
  // messenger.hpp keep resolving cvm::topology::loc_t without pulling in the
  // topology lookup declarations.
  namespace topology {

    using location_defs::loc_t;
    using location_defs::null;

  }

}
