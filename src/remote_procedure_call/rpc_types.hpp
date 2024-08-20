// RPC that uses type of the template to call a function

#pragma once

#include <typeindex>
#include <tuple>

#include "cvm/topology.hpp"
#include "cvm/logger.hpp"
#include "cvm/type_traits.hpp"

#define MakeRPC(name, func_type) \
struct name : cvm::rpc_function<func_type> {};

namespace cvm {

  template <typename T>
  struct rpc_function {
    using function_type = T;
  };

  template <typename T>
  struct function_traits;

  // template<typename... Args>
  // struct function_args { };

  template <typename R, typename... Args>
  struct function_traits <R (Args...)> {
    using return_type = R;
    // using argument_types = function_args<Args...>;
  };

  template <typename T>
  using return_type_t = typename function_traits<typename T::function_type>::return_type;

  class rpc {

    class remote_call_base {
      public:
        virtual ~remote_call_base () {};
    };

    template <typename F>
    class remote_call : public remote_call_base {
      // represents a group of functions that listen to a type F from various locations

      public:
        virtual ~remote_call() { };

        typedef std::function<typename F::function_type> listener_t;

        void add_listener(cvm::topology::loc_t loc, F::function_type f) {
          assert((!listeners_.contains(loc)) && "listener already exists");

          listeners_[loc] = f;
          cvm::log(cvm::DEBUG, "[remote_procedure_call] added listener to loc {}, type {} has {} listeners\n", loc, cvm::type_traits::name<decltype(f)>(), listeners_.size());
        }

        template <typename... Args>
        return_type_t<F> run(cvm::topology::loc_t loc, __attribute__((unused)) Args... args) {
          // Run a listener for a specific location with the given args
          auto f = listeners_[loc];
          cvm::log(cvm::DEBUG, "[remote_procedure_call] running listener at loc {}, type {}\n", loc, cvm::type_traits::name<decltype(f)>());
          return f(args...);
        }

      private:
        std::unordered_map<cvm::topology::loc_t, listener_t> listeners_;

    };

    public:
      
      template<typename F>
      void connect(cvm::topology::loc_t loc, F::function_type listener) {
        // Connect a listener to the appropriate remote_call
        cvm::log(cvm::DEBUG, "[remote_procedure_call] connect location {} with type {}\n", loc, typeid(F).name());
        assert((loc != cvm::topology::null) && "attempting to connect to a null location");
        remote_calls<F>()->add_listener(loc, listener);
      }

      template<typename F, typename... Args>
      return_type_t<F> signal(cvm::topology::loc_t loc, Args... args) {
        // Find a remote call and call it's function for a specified location
        cvm::log(cvm::DEBUG, "[remote_procedure_call] signal location {} with type {}\n", loc, typeid(F).name());
        assert((loc != cvm::topology::null) && "attempting to signal to a null location");
        return remote_calls<F>()->template run<Args...>(loc, args...);
      }

    private:

      template <typename F>
      std::shared_ptr<remote_call<F>> remote_calls() {
        auto key = std::type_index(typeid(F));
        std::lock_guard<std::mutex> guard(remote_calls_mutex_);
        auto it = remote_calls_.find(key);
        if (it == remote_calls_.end()) {
          std::shared_ptr<remote_call<F>> rc = std::make_shared<remote_call<F>>();
          it = remote_calls_.emplace(key, std::move(rc)).first;
        } 
        return std::dynamic_pointer_cast<remote_call<F>>(it->second);
      }


      std::unordered_map<std::type_index, std::shared_ptr<remote_call_base>> remote_calls_;
      std::mutex remote_calls_mutex_;
  };

}