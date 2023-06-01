#pragma once

#include <cassert>
#include <list>
#include <functional>
#include "cvm/messenger.hpp"
#include "cvm/callbacks.hpp"
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
      inline static int all = -1;
      inline static messengerr messenger;
      inline static callbackss callbacks;

      // generic interface
      // register classes during static init
      template<typename T, typename... Args>
      static bool regist(const std::string& module, int id, Args&&... args) {
        static std::list<T> objs_;

        bool from_hierarchy = module.find('.') != std::string::npos;

        if (id == all) {
          std::vector<cvm::topology::loc_t> locs;
          if (from_hierarchy)
            locs = cvm::topology::get_from_hierarchy(module);
          else
            locs = cvm::topology::get_from_type(module);

          if (locs.empty())
            return false;

          constructors().push_back(
            [locs, ...args = std::forward<Args>(args)] () {
              for (const auto& loc : locs)
                objs_.emplace_back(loc, objs_.size(), args...); });
        }
        else {
          cvm::topology::loc_t loc;

          if (from_hierarchy)
            loc = cvm::topology::get_from_hierarchy(module, id);
          else
            loc = cvm::topology::get_from_type(module, id);

          if (loc == cvm::topology::null)
            return false;

          constructors().push_back(
            [loc, ...args = std::forward<Args>(args)] () { objs_.emplace_back(loc, objs_.size(), args...); });
        }

        destructors().push_back([] () { return objs_.clear(); });
        return true;
      }

      static void build() {
        for (const auto& construct : constructors())
          construct();
      }

      static void shutdown() {
        messenger.clear();
        callbacks.clear();
        for (const auto& destruct : destructors())
          destruct();
      }
  };
}

// this should be used in source file
// presumably, objects will subscribe to transactions in constructor
#define REGISTRY_register(type, module, id, ...) \
  namespace _registry { \
    auto type##_register = []() -> bool { return cvm::registry::regist<type>( #module, id __VA_OPT__(,) __VA_ARGS__); }; \
    bool type##_SUCCESS = type##_register(); \
  }
