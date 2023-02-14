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
    struct relation {
      std::string leaf;
      int id = -1;
    };

    template<typename T>
    static bool regist(const struct relation& relat) {
      static std::vector<T&> objs_;

      if (relat.id == -1) {
        auto locs = cvm::topology::get(relat.leaf);
        for (const auto& loc : locs)
          if (loc == cvm::topology::null)
            return false;

          objs_.emplace(std::piecewise_construct,
                        std::tuple(loc, objs_.size()));
      }
      else {
        auto loc = cvm::topology::get(relat.leaf, relat.id);
        if (loc == cvm::topology::null)
          return false;

        objs_.emplace(std::piecewise_construct,
                      std::tuple(loc, objs_.size()));
      }

      return true;
    }
  }
}

// this should be used in source file
// presumably, objects will subscribe to transactions in constructor
#define REGISTRY_TOPOLOGY(type, relation) \
  auto type##_SUCCESS = [&]() -> bool { return cvm::registry::regist<##type##>(##relation##); }
