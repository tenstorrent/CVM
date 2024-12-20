#pragma once

#include <cassert>
#include <list>
#include <functional>
#include <typeinfo>
#include <unordered_set>
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

      static auto& configures() {
        static std::vector<std::function<void()>> configures_;
        return configures_;
      }

      static auto& checks() {
        static std::vector<std::function<void()>> checks_;
        return checks_;
      }

      static auto& destructors() {
        static std::vector<std::function<void()>> destructs_;
        return destructs_;
      }

      static auto& shutdown_readys() {
        static std::vector<std::function<bool()>> sr_;
        return sr_;
      }

      static auto& registered() {
        static std::unordered_set<std::string> registered_;
        return registered_;
      }

    public:

      // ex.
      // { leaf = "core", id = -1 }
      // will instantiate a checker for every core
      // core   -   core   -   core
      //   |          |          |
      // checker    checker    checker
      inline static int all = -1;
      inline static messenger messenger;
      inline static callbacks callbacks;

      // register classes during static init
      template<typename T, bool A, typename... Args>
      static bool regist(const std::string& module, int id, Args&&... args) {
        static std::list<T> objs_;

        if constexpr (!A) {
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
              [&, loc, ...args = std::forward<Args>(args)] () { objs_.emplace_back(loc, objs_.size(), args...); });
          }
        }
        else {
          constructors().push_back(
            [&, ...args = std::forward<Args>(args)] () { objs_.emplace_back(args...); });
        }

        if constexpr (requires(T& t) { t.configure(); }) {
          configures().push_back([&] () { for (auto& t : objs_) { t.configure(); }});
        }

        if constexpr (requires(T& t) { t.check(); }) {
          checks().push_back([&] () { for (auto& t : objs_) t.check(); });
        }

        if constexpr (requires(T& t) {{ t.shutdown_ready() } -> std::same_as<bool>;}) {
          shutdown_readys().push_back([&] () -> bool {
              bool ready = true;
              for (auto& t : objs_)
                ready = ready and t.shutdown_ready();
              return ready;
            });
        }

        destructors().push_back([&] () { return objs_.clear(); });
        registered().emplace(typeid(T).name());
        return true;
      }

      static void build() {
        // in case something was signalled between the last clear and this build
        // eg, if emulation has a DPI that's called after shutdown but before build
        messenger.clear();
        callbacks.clear();

        messenger.build();
        callbacks.build();
        for (const auto& construct : constructors())
          construct();
      }

      static void configure() {
        for (const auto& configure : configures())
          configure();
      }

      static void check() {
        auto g = messenger.task_guard();
        for (const auto& check : checks())
          check();
      }

      static bool shutdown() {
        // handshake with each registry component first
        bool ready = true;

        {
            auto g = messenger.task_guard();
            for (const auto& shutdown_ready : shutdown_readys())
              ready = ready and shutdown_ready();
        }

        if (not ready)
          return false;

        // messenger.clear() needs to be called before callbacks.clear()
        // messenger may cause new callbacks to be pushed
        // callbacks shouldn't have an (immediate) effect on messenger
        messenger.clear();
        if (!callbacks.clear()) return false;
        for (const auto& destruct : destructors())
          destruct();
        return true;
      }

      template <typename T>
      static bool is_registered() {
        return registered().count(typeid(T).name());
      }
  };
}

#define REGISTRY_CONCAT_IMPL(x, y) x##y
#define REGISTRY_CONCAT(x, y) REGISTRY_CONCAT_IMPL(x, y)

namespace _registry {
  template<typename> struct RemoveBrackets;
  template<typename T> struct RemoveBrackets<void (T)> {
      typedef T Type;
  };
}

// this should be used in source file
// presumably, objects will subscribe to transactions in constructor
#define REGISTRY_register(type, module, id, ...) \
    namespace _registry { \
      static bool REGISTRY_CONCAT(_, __COUNTER__) = std::invoke([]() -> bool { return cvm::registry::regist<RemoveBrackets<void (type)>::Type, false>( #module, id __VA_OPT__(,) __VA_ARGS__); }); \
    }

#define REGISTRY_register_topology_agn(type, ...) \
    namespace _registry { \
      static bool REGISTRY_CONCAT(_, __COUNTER__) = std::invoke([]() -> bool { return cvm::registry::regist<RemoveBrackets<void (type)>::Type, true>( "", 0 __VA_OPT__(,) __VA_ARGS__); }); \
    }
