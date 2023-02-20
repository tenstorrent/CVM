#include <cassert>
#include <list>
#include <functional>
#include "cvm/topology.hpp"

namespace cvm {

  class registry {

    private:

      // to deal with out-of-order static init
      static auto& constructors() {
        static std::vector<std::function<void()>> constructs_;
        return constructs_;
      }

      static auto& destructors() {
        static std::vector<std::function<void()>> destructs_;
        return destructs_;
      }

    public:

      // ex.
      // { leaf = "core", id = -1 }
      // will instantiate a checker for every core
      // core   -   core   -   core
      //   |          |          |
      // checker    checker    checker
      inline static constexpr int all = -1;

      // register classes during static init
      template<typename T>
      static bool regist(const std::string& module, int id) {
        static std::list<T> objs_;

        if (module == "top") {
          auto loc = cvm::topology::get(module, 0);
          if (loc == cvm::topology::null)
            return false;

          constructors().push_back(
            [loc] () { objs_.emplace_back(loc, objs_.size()); });
        }
        else if (id == all) {
          auto locs = cvm::topology::get(module);
          if (locs.empty())
            return false;

          constructors().push_back(
            [locs] () {
              for (const auto& loc : locs)
                objs_.emplace_back(loc, objs_.size()); });
        }
        else {
          auto loc = cvm::topology::get(module, id);
          if (loc == cvm::topology::null)
            return false;

          constructors().push_back(
            [loc] () { objs_.emplace_back(loc, objs_.size()); });
        }

        destructors().push_back([] () { return objs_.clear(); });
        return true;
      }

      static void reset() {
        for (const auto& destruct : destructors())
          destruct();
        for (const auto& construct : constructors())
          construct();
      }
  };
}

// this should be used in source file
// presumably, objects will subscribe to transactions in constructor
#define REGISTRY_register(type, module, id) \
  auto type##_register = []() -> bool { return cvm::registry::regist<type>( #module, id); }; \
  bool type##_SUCCESS = type##_register();
