#pragma once

#include <cassert>
#include "cvm/topology.hpp"

namespace cvm {

    template <typename T>
        class messenger {

            public:

                typedef std::function<void(const T&)> listener;
                // for topology dependent signaling (i.e. SV to C++)
                inline static std::unordered_map<cvm::topology::loc_t, std::vector<listener>> signals_;

                // assume hierarchical path
                static void connect(cvm::topology::loc_t loc, const listener& l) {
                  assert(loc != cvm::topology::null);
                  signals_[loc].push_back(l);
                }

                static void signal(cvm::topology::loc_t loc, const T& t) {
                  assert(loc != cvm::topology::null);
                  if (signals_.count(loc)) {
                    for (const auto& func : signals_.at(loc))
                      func(t);
                  }
                }
        };
}
