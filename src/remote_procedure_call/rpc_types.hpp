// RPC that uses type of the template to call a function

#pragma once

#include <typeindex>
#include <tuple>

#include "cvm/topology.hpp"
#include "cvm/logger.hpp"
#include "cvm/type_traits.hpp"

namespace cvm {

  // template<typename... Args>
  // struct rpc_args { };

  // // TODO: Use this or std::function?
  // template<typename R, typename... Args>
  // struct rpc_function { 
  //   using function_type = R;
  //   using args = rpc_args<Args...>;
  // };

  template<typename... Args>
  struct pack { };

  template <typename T>
  struct rpc_function {
    // using function_type = std::function<T (Args...)>;
    using function_type = T;
    // using args = pack<Args...>;
  };

  template <typename T>
  struct function_traits;

  template<typename... Args>
  struct function_args { };

  template <typename R, typename... Args>
  struct function_traits <R (Args...)> {
    using return_type = R;
    using argument_types = function_args<Args...>;
  };

  template <typename T>
  using argument_types_t = typename function_traits<typename T::function_type>::argument_types;
  
  template <typename T>
  using return_type_t = typename function_traits<typename T::function_type>::return_type;

  template<std::size_t N, typename... T, std::size_t... I>
  std::tuple<std::tuple_element_t<N+I, std::tuple<T...>>...>
  sub(std::index_sequence<I...>);

  template<std::size_t N, typename... T>
  using subpack = decltype(sub<N, T...>(std::make_index_sequence<sizeof...(T) - N>{}));

  class rpc {

    class remote_call_base {
      public:
        virtual ~remote_call_base () {};
    };

    template <typename F, typename... Args>
    class remote_call : public remote_call_base {
      // represents a group of functions that listen to a type F from various locations

      public:
        virtual ~remote_call() {
          std::cout << "remote call destroyed" << std::endl;
        };

        typedef std::function<typename F::function_type> listener_t;
        //typedef F::function_type listener_t;

        void add_listener(cvm::topology::loc_t loc, F::function_type f) {
          listeners_[loc] = f;
          cvm::log(cvm::DEBUG, "[remote_procedure_call] added listener to loc {}, type {} has {} listeners\n", loc, cvm::type_traits::name<decltype(f)>(), listeners_.size());
          // /std::cout << "add_listener loc " << loc << " type " << cvm::type_traits::name<decltype(f)>() << std::endl;
          // /std::cout << "add_listener listeners_ size " << listeners_.size() << std::endl; 
        }

        return_type_t<F> run(cvm::topology::loc_t loc, __attribute__((unused)) Args... args) {
          // Run a listener for a specific location with the given args
          // /std::cout << "run loc " << loc << std::endl;
          // /std::cout << "listeners_ size " << listeners_.size() << std::endl;

          auto it = listeners_.find(loc);   // TODO: this is seg faulting!!
          if (it == listeners_.end()) {
            // /std::cout << "assertion!" << std::endl;
            assert (false && "invalid location in run\n");
          }

          listener_t f = it->second;

          cvm::log(cvm::DEBUG, "[remote_procedure_call] run listener at loc {}, type {}\n", loc, cvm::type_traits::name<decltype(f)>());
          // /std::cout << "run found " << cvm::type_traits::name<decltype(f)>() << std::endl;

          // cut the first arg out


          // return f(args...);

          return 5;
        }

      private:

        // template <typename A1, typename... OtherArgs>
        // return_type_t<F> run_(cvm::topology::loc_t loc, OtherArgs... args) {
        //   return listeners_[loc](args...);
        // }

        std::unordered_map<cvm::topology::loc_t, listener_t> listeners_;

    };

    public:
      
      template<typename F, typename... Args>
      void connect(cvm::topology::loc_t loc, F::function_type listener) {
        // Connect a listener to the appropriate remote_call
        // /std::cout << "connect" << std::endl;
        cvm::log(cvm::DEBUG, "[remote_procedure_call] connect location {}\n", loc);

        auto rc = remote_calls<F, Args...>();
        std::cout << "rc in connect: " << rc << std::endl;
        // /std::cout << "F type ID in connect: " << typeid(F).name() << std::endl;
        rc->add_listener(loc, listener);
        // remote_calls<F, Args...>()->add_listener(loc, listener);
      }

      template<typename F, typename... Args>
      return_type_t<F> signal(__attribute__((unused)) cvm::topology::loc_t loc, __attribute__((unused)) Args... args) {
        // Find a remote call and call it's function for a specified location

        // /std::cout << "signal" << std::endl;

        // printf("%u\n", sizeof...(args));
        // // /std::cout << sizeof...(args) << std::endl;   // this prints 3

        // return 5;
        auto rc = remote_calls<F, Args...>();
        std::cout << "rc in signal: " << rc << std::endl;
        // /std::cout << "F type ID in signal: " << typeid(F).name() << std::endl;
        return rc->run(loc, args...);
        // return remote_calls<F, Args...>()->run(loc, args...);
      }

    private:

      template <typename F, typename... Args>
      std::shared_ptr<remote_call<F, Args...>> remote_calls() {
        auto key = std::type_index(typeid(F));
        std::lock_guard<std::mutex> guard(remote_calls_mutex_);
        auto it = remote_calls_.find(key);
        std::cout << "key " << typeid(F).name() << " in remote_calls? " << remote_calls_.contains(key) << std::endl;
        if (it == remote_calls_.end()) {
          std::shared_ptr<remote_call<F, Args...>> rc = std::make_shared<remote_call<F, Args...>>();
          it = remote_calls_.emplace(key, std::move(rc)).first;
        } 
        auto rc_ptr = std::dynamic_pointer_cast<remote_call<F, Args...>>(it->second);
          std::cout << "stored type: " << typeid((it->second)).name() << std::endl;
        if (rc_ptr == nullptr) {
          std::cout << "rc_ptr is null" << std::endl;
        }
        else {
          std::cout << "rc_ptr is " << rc_ptr << std::endl;
        }
        return rc_ptr;
        // return std::dynamic_pointer_cast<remote_call<F, Args...>>(it->second);
      }


      std::unordered_map<std::type_index, std::shared_ptr<remote_call_base>> remote_calls_;
      std::mutex remote_calls_mutex_;
  };

}