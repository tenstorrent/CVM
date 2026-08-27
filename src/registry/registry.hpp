// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <cassert>
#include <functional>
#include <string_view>
#include <typeinfo>
#include <unordered_set>
#include <vector>
#include <list>
#include <ranges>
#include "cvm/messenger.hpp"
#include "cvm/callbacks.hpp"
#include "cvm/topology.hpp"

namespace _registry {

  // A string literal cannot be a non-type template parameter directly; this
  // wrapper is a structural type, so the module path a registration names can
  // be carried into the template and resolved against the topology.
  template <size_t N>
  struct StringLiteral {
    constexpr StringLiteral(const char (&literal)[N]) { std::copy_n(literal, N, value); }

    constexpr operator std::string_view() const { return std::string_view(value, N - 1); }

    char value[N];
  };

}

namespace cvm {

  class registry {

    private:

      template <typename T>
      struct identity {};

      // We should probably do CRTP-style instead.
      class meta_helper {

        public:

          template <typename T, typename... Args>
          meta_helper([[maybe_unused]] identity<T> an, cvm::topology::loc_t loc, unsigned id, unsigned reset_domain, Args&&... args)
            : loc_(loc), reset_domain_(reset_domain), obj_(nullptr, []([[maybe_unused]] void* obj) {}) {

            construct_ = [this, loc, id, ...args = std::forward<Args>(args)]() {
              obj_ = std::unique_ptr<void, void(*)(void*)>(
                       new T(loc, id, args...),
                       [](void* obj) { delete reinterpret_cast<T*>(obj); }
                     );
            };

            destruct_ = [this]() {
              obj_.reset();
            };

            if constexpr (requires(T& t) { t.configure(); }) {
              configure_ = [this]() { (*reinterpret_cast<T*>(obj_.get())).configure(); };
            }

            if constexpr (requires(T& t) { t.check(); }) {
              check_ = [this]() { (*reinterpret_cast<T*>(obj_.get())).check(); };
            }

            if constexpr (requires(T& t) {{ t.shutdown_ready() } -> std::same_as<bool>;}) {
              sr_ = [this]() -> bool { return (*reinterpret_cast<T*>(obj_.get())).shutdown_ready(); };
            }
          }

          cvm::topology::loc_t loc_;
          unsigned reset_domain_;
          std::unique_ptr<void, void(*)(void*)> obj_;

          // Helper functions.
          std::function<void()> construct_ = nullptr;
          std::function<void()> destruct_ = nullptr;
          std::function<void()> configure_ = nullptr;
          std::function<void()> check_ = nullptr;
          std::function<bool()> sr_ = nullptr;
      };

      template <typename T>
      static int type_nums() {
        static int id = -1;
        return ++id;
      }

      static auto& components() {
        static std::list<meta_helper> components_;
        return components_;
      };

      static auto& registered() {
        static std::unordered_set<std::string> registered_;
        return registered_;
      }

      static void build(std::ranges::view auto&& components) {
        // in case something was signalled between the last clear and this build
        // eg, if emulation has a DPI that's called after shutdown but before build
        messenger.clear();
        callbacks.clear();

        messenger.build();
        callbacks.build();

        for (auto& c : components)
          c.construct_();
      }

      static bool shutdown(std::ranges::view auto&& components) {
        // handshake with each registry component first
        bool ready = true;

        {
            auto g = messenger.task_guard();
            for (auto& c : components) {
              if (c.sr_)
                ready = ready and c.sr_();
            }
        }

        if (not ready)
          return false;

        // messenger.clear() needs to be called before callbacks.clear()
        // messenger may cause new callbacks to be pushed
        // callbacks shouldn't have an (immediate) effect on messenger
        messenger.clear();
        if (!callbacks.clear()) return false;
        for (auto& c : components)
          c.destruct_();
        return true;
      }

    public:

      inline static constexpr int all = -1;
      inline static ::cvm::messenger messenger;
      inline static ::cvm::callbacks callbacks;

      // Whether a module path names anything in the given topology. A path
      // containing a '.' is matched against the hierarchy, otherwise against
      // the type table, which is how the runtime regist() reads it too.
      template<typename Topology, _registry::StringLiteral module, int id>
      static constexpr bool resolvable() {
        constexpr std::string_view path = module;
        constexpr bool from_hierarchy = path.find('.') != std::string_view::npos;

        if constexpr (id == all)
          return from_hierarchy ? Topology::hierarchy_exists(path) : Topology::type_exists(path);
        else
          return (from_hierarchy ? Topology::location_of_hierarchy(path, id)
                                 : Topology::location_of_type(path, id)) != Topology::null;
      }

      // Compile-time counterpart of regist(). The module path and instance id
      // are template parameters, so the locations are resolved against the
      // topology during translation rather than looked up at start-up.
      //
      // A path the topology does not contain is skipped, matching regist()'s
      // false return; instantiate with required = true to make it a
      // translation error instead.
      template<typename Topology, typename T, _registry::StringLiteral module, int id, bool required, typename... Args>
      static bool regist_static(Args&&... args) {
        constexpr std::string_view path = module;
        constexpr bool from_hierarchy = path.find('.') != std::string_view::npos;
        constexpr bool found = resolvable<Topology, module, id>();

        static_assert(found or not required,
                      "registered module path does not exist in this topology");

        if constexpr (not found) {
          return false;
        }
        else if constexpr (id == all) {
          constexpr auto locs = from_hierarchy ? Topology::locations_of_hierarchy(path)
                                               : Topology::locations_of_type(path);

          for (const auto& loc : locs)
            components().emplace_back(identity<T>{}, loc, type_nums<T>(), args...);

          registered().emplace(typeid(T).name());
          return true;
        }
        else {
          constexpr auto loc = from_hierarchy ? Topology::location_of_hierarchy(path, id)
                                              : Topology::location_of_type(path, id);

          components().emplace_back(identity<T>{}, loc, type_nums<T>(), args...);

          registered().emplace(typeid(T).name());
          return true;
        }
      }

      template<typename T, typename... Args>
      static bool regist(const std::string& module, int id, Args&&... args) {
        bool from_hierarchy = module.find('.') != std::string::npos;
        if (id == all) {
          std::vector<cvm::topology::loc_t> locs;
          if (from_hierarchy)
            locs = cvm::topology::get_from_hierarchy(module);
          else
            locs = cvm::topology::get_from_type(module);

          if (locs.empty())
            return false;

          for (const auto& loc : locs)
            components().emplace_back(identity<T>{}, loc, type_nums<T>(), args...);
        }
        else {
          cvm::topology::loc_t loc;

          if (from_hierarchy)
            loc = cvm::topology::get_from_hierarchy(module, id);
          else
            loc = cvm::topology::get_from_type(module, id);

          if (loc == cvm::topology::null)
            return false;

          components().emplace_back(identity<T>{}, loc, type_nums<T>(), args...);
        }

        registered().emplace(typeid(T).name());
        return true;
      }

      static void build() {
        return build(std::ranges::ref_view(components()));
      }

      static void build(cvm::topology::loc_t loc) {
        return build(std::ranges::filter_view(components(), [loc](auto& c) { return c.loc_ == loc; }));
      }

      static void build_all_except(cvm::topology::loc_t loc) {
        return build(std::ranges::filter_view(components(), [loc](auto& c) { return c.loc_ != loc; }));
      }

      static void build_all_except(const std::unordered_set<cvm::topology::loc_t>& loc) {
        return build(std::ranges::filter_view(components(), [loc](auto& c) { return not loc.contains(c.loc_); }));
      }

      static void build_domain(unsigned reset_domain) {
        return build(std::ranges::filter_view(components(), [reset_domain](auto& c) { return c.reset_domain_ == reset_domain; }));
      }

      static void configure() {
        for (auto& c : components())
          if (c.configure_)
            c.configure_();
      }

      static void check() {
        auto g = messenger.task_guard();
        for (auto& c : components())
          if (c.check_)
            c.check_();
      }

      static bool shutdown() {
        return shutdown(std::ranges::ref_view(components()));
      }

      static bool shutdown(cvm::topology::loc_t loc) {
        return shutdown(std::ranges::filter_view(components(), [loc](auto& c) { return c.loc_ == loc; }));
      }

      static bool shutdown_all_except(cvm::topology::loc_t loc) {
        return shutdown(std::ranges::filter_view(components(), [loc](auto& c) { return c.loc_ != loc; }));
      }

      static bool shutdown_all_except(const std::unordered_set<cvm::topology::loc_t>& loc) {
        return shutdown(std::ranges::filter_view(components(), [loc](auto& c) { return not loc.contains(c.loc_); }));
      }

      static bool shutdown_domain(unsigned reset_domain) {
        return shutdown(std::ranges::filter_view(components(), [reset_domain](auto& c) { return c.reset_domain_ == reset_domain; }));
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
#define REGISTRY_register_with_reset_(type, module, id, reset_domain, ...) \
    namespace _registry { \
      static bool REGISTRY_CONCAT(_, __COUNTER__) = std::invoke([]() -> bool { return cvm::registry::regist<RemoveBrackets<void (type)>::Type>( #module, id, reset_domain __VA_OPT__(,) __VA_ARGS__); }); \
    }
#define REGISTRY_register_with_reset(...) REGISTRY_register_with_reset_(__VA_ARGS__)

// if reset domain not specified, default to 0
#define REGISTRY_register_(type, module, id, ...) \
    namespace _registry { \
      static bool REGISTRY_CONCAT(_, __COUNTER__) = std::invoke([]() -> bool { return cvm::registry::regist<RemoveBrackets<void (type)>::Type>( #module, id, 0 __VA_OPT__(,) __VA_ARGS__); }); \
    }
#define REGISTRY_register(...) REGISTRY_register_(__VA_ARGS__) // allows using ENUMS / #defines instead of numbers

// Compile-time registration. Resolves the module path against
// cvm::static_topology during translation, so the expanding source file must
// include the topology it is built against:
//
//   #include "cvm/topology_defs.hpp"
//
// which is what depending on a registry_gen target rather than @cvm//:registry
// provides. The _required forms reject a path the topology does not contain;
// the plain forms skip it, as the runtime macros do.
#define CVM_REGISTRY_REGISTER_(type, module, id, reset_domain, required, ...) \
    namespace _registry { \
      static bool REGISTRY_CONCAT(_, __COUNTER__) = std::invoke([]() -> bool { return cvm::registry::regist_static<cvm::static_topology, RemoveBrackets<void (type)>::Type, #module, id, required>(reset_domain __VA_OPT__(,) __VA_ARGS__); }); \
    }

#define CVM_REGISTRY_register_(type, module, id, ...) \
    CVM_REGISTRY_REGISTER_(type, module, id, 0, false __VA_OPT__(,) __VA_ARGS__)
#define CVM_REGISTRY_register(...) CVM_REGISTRY_register_(__VA_ARGS__)

#define CVM_REGISTRY_register_required_(type, module, id, ...) \
    CVM_REGISTRY_REGISTER_(type, module, id, 0, true __VA_OPT__(,) __VA_ARGS__)
#define CVM_REGISTRY_register_required(...) CVM_REGISTRY_register_required_(__VA_ARGS__)

#define CVM_REGISTRY_register_with_reset_(type, module, id, reset_domain, ...) \
    CVM_REGISTRY_REGISTER_(type, module, id, reset_domain, false __VA_OPT__(,) __VA_ARGS__)
#define CVM_REGISTRY_register_with_reset(...) CVM_REGISTRY_register_with_reset_(__VA_ARGS__)

#define CVM_REGISTRY_register_required_with_reset_(type, module, id, reset_domain, ...) \
    CVM_REGISTRY_REGISTER_(type, module, id, reset_domain, true __VA_OPT__(,) __VA_ARGS__)
#define CVM_REGISTRY_register_required_with_reset(...) CVM_REGISTRY_register_required_with_reset_(__VA_ARGS__)
