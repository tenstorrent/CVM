#pragma once

#include <cassert>
#include <unordered_map>
#include <vector>
#include <functional>
#include <typeinfo>
#include <typeindex>
#include "cvm/topology.hpp"

namespace cvm {

      class messengerr {

          public:

              typedef std::function<void(const void*)> listener;
              typedef std::unordered_map<cvm::topology::loc_t, std::vector<listener>> per_type;
              std::unordered_map<std::type_index, per_type> signals_;

              // assume topology
              template<typename T>
              void connect(cvm::topology::loc_t loc, const std::function<void(const T&)>& l) {
                  assert(loc != cvm::topology::null);
                  auto& per = signals_[std::type_index(typeid(T))];
                  per[loc].push_back([l] (const void* p) {
                      auto pT = static_cast<const T*>(p);
                      l(*pT); });
              }

              template<typename T>
              void signal(cvm::topology::loc_t loc, const T& t) {
                  assert(loc != cvm::topology::null);
                  auto& per = signals_[std::type_index(typeid(T))];
                  auto it = per.find(loc);
                  if (it != loc.end()) {
                      auto p = static_cast<const void*>(&t);
                      for (const auto& func : it.second)
                          func(p);
                  }
              }

              void clear() {
                  signals_.clear();
              }
      };
}
