#pragma once

#include <cassert>
#include <functional>
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
          meta_helper([[maybe_unused]] identity<T> an, cvm::topology::loc_t loc, unsigned id, Args&&... args)
            : loc_(loc), obj_(nullptr, []([[maybe_unused]] void* obj) {}) {

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

          cvm::topology::loc_t loc_ = 0;
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

      inline static int all = -1;
      inline static messenger messenger;
      inline static callbacks callbacks;

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

      // TODO: We can support hierarchical path resets. That is, given an id,
      // perform operation on all underlying hierarchical paths.
      static void build(cvm::topology::loc_t loc) {
        return build(std::ranges::filter_view(components(), [loc](auto& c) { return c.loc_ == loc; }));
      }

      // Temporary until domain support.
      static void build_all_except(cvm::topology::loc_t loc) {
        return build(std::ranges::filter_view(components(), [loc](auto& c) { return c.loc_ != loc; }));
      }

      static void build_all_except(const std::unordered_set<cvm::topology::loc_t>& loc) {
        return build(std::ranges::filter_view(components(), [loc](auto& c) { return not loc.contains(c.loc_); }));
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
      static bool REGISTRY_CONCAT(_, __COUNTER__) = std::invoke([]() -> bool { return cvm::registry::regist<RemoveBrackets<void (type)>::Type>( #module, id __VA_OPT__(,) __VA_ARGS__); }); \
    }
