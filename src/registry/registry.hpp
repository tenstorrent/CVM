// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <functional>
#include <utility>
#include <string_view>
#include <typeinfo>
#include <unordered_set>
#include <vector>
#include <list>
#include <ranges>
#include "cvm/messenger.hpp"
#include "cvm/callbacks.hpp"
#include "cvm/topology.hpp"

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

      template<typename Topology>
      static constexpr bool resolvable(std::string_view path, int id) {
        const bool from_hierarchy = path.find('.') != std::string_view::npos;

        if (id == all)
          return from_hierarchy ? Topology::hierarchy_exists(path) : Topology::type_exists(path);
        else
          return (from_hierarchy ? Topology::get_from_hierarchy(path, id)
                                 : Topology::get_from_type(path, id)) != Topology::null;
      }

      // Rebuilds the lookup key from a mods node: the node's path, plus the
      // "[g]" suffix when a group was selected through operator[]. Static
      // storage so the string_view handed to the tables stays valid.
      template<auto module_node>
      struct module_key {
        static constexpr auto storage = [] {
          constexpr std::string_view base = module_node.path;
          std::array<char, base.size() + 13> buf{};
          size_t n = 0;
          for (char c : base) buf[n++] = c;
          if (module_node.group >= 0) {
            buf[n++] = '[';
            char digits[12]{};
            size_t d = 0;
            int g = module_node.group;
            do { digits[d++] = static_cast<char>('0' + g % 10); g /= 10; } while (g > 0);
            while (d > 0) buf[n++] = digits[--d];
            buf[n++] = ']';
          }
          return std::pair{buf, n};
        }();

        static constexpr std::string_view value{storage.first.data(), storage.second};
      };

      // module_node is a node of Topology::mods, so a mistyped path fails
      // member lookup at the macro expansion; the assert covers an id or
      // group the topology does not have.
      template<typename Topology, typename T, auto module_node, int id, typename... Args>
      static bool regist_static(Args&&... args) {
        constexpr std::string_view path = module_key<module_node>::value;
        constexpr bool from_hierarchy = path.find('.') != std::string_view::npos;

        static_assert(resolvable<Topology>(path, id),
                      "registered module path does not exist in this topology");

        if constexpr (id == all) {
          constexpr auto locs = from_hierarchy ? Topology::get_from_hierarchy(path)
                                               : Topology::get_from_type(path);

          for (const auto& loc : locs)
            components().emplace_back(identity<T>{}, loc, type_nums<T>(), args...);

          registered().emplace(typeid(T).name());
          return true;
        }
        else {
          constexpr auto loc = from_hierarchy ? Topology::get_from_hierarchy(path, id)
                                              : Topology::get_from_type(path, id);

          components().emplace_back(identity<T>{}, loc, type_nums<T>(), args...);

          registered().emplace(typeid(T).name());
          return true;
        }
      }

      template<typename T, typename... Args>
      static bool regist(const std::string& module_path, int id, Args&&... args) {
        bool from_hierarchy = module_path.find('.') != std::string::npos;
        if (id == all) {
          std::vector<cvm::topology::loc_t> locs;
          if (from_hierarchy)
            locs = cvm::topology::get_from_hierarchy(module_path);
          else
            locs = cvm::topology::get_from_type(module_path);

          if (locs.empty())
            return false;

          for (const auto& loc : locs)
            components().emplace_back(identity<T>{}, loc, type_nums<T>(), args...);
        }
        else {
          cvm::topology::loc_t loc;

          if (from_hierarchy)
            loc = cvm::topology::get_from_hierarchy(module_path, id);
          else
            loc = cvm::topology::get_from_type(module_path, id);

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

#define REGISTRY_register_with_reset_(type, module_path, id, reset_domain, ...) \
    namespace _registry { \
      static bool REGISTRY_CONCAT(_, __COUNTER__) = std::invoke([]() -> bool { return cvm::registry::regist_static<cvm::static_topology, RemoveBrackets<void (type)>::Type, cvm::static_topology::mods().module_path, id>(reset_domain __VA_OPT__(,) __VA_ARGS__); }); \
    }
#define REGISTRY_register_with_reset(...) REGISTRY_register_with_reset_(__VA_ARGS__)

// id accepts ENUMS / #defines / constexpr ints; reset domain defaults to 0
#define REGISTRY_register_(type, module_path, id, ...) \
    REGISTRY_register_with_reset_(type, module_path, id, 0 __VA_OPT__(,) __VA_ARGS__)
#define REGISTRY_register(...) REGISTRY_register_(__VA_ARGS__)
