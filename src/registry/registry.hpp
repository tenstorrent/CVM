#include <cassert>
#include "cvm/topology.hpp"

namespace cvm {
  namespace registry {
    // ex.
    // { leaf = "core", id = -1 }
    // will instantiate a checker for every core
    // core   -   core   -   core
    //   |          |          |
    // checker    checker    checker
    inline constexpr int all = -1;

    template<typename T>
    bool regist(const std::string& module, int id) {
      static std::vector<T> objs_;

      if (module == "top") {
        auto loc = cvm::topology::get(module, 0);
        assert(loc != cvm::topology::null);
        objs_.emplace(objs_.end(), loc, objs_.size());
      }
      else if (id == -1) {
        auto locs = cvm::topology::get(module);
        for (const auto& loc : locs)
          objs_.emplace(objs_.end(), loc, objs_.size());
      }
      else {
        auto loc = cvm::topology::get(module, id);
        if (loc == cvm::topology::null)
          return false;

        objs_.emplace(objs_.end(), loc, objs_.size());
      }

      return true;
    }
  }
}

// this should be used in source file
// presumably, objects will subscribe to transactions in constructor
#define REGISTRY_register(type, module, id) \
  auto type##_register = []() -> bool { return cvm::registry::regist<type>( #module, id); }; \
  bool type##_SUCCESS = type##_register();
