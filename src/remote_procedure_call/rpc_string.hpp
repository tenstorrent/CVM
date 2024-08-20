// RPC but uses string to match function 

// Register takes function name, turns it into string, then adds it to map of functions

// Use templates for the variable number of args? i think idk hmm
#pragma once 

#include <atomic>
#include <cassert>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>

#include "cvm/topology.hpp"
#include "cvm/logger.hpp"

namespace cvm {

  template<typename... Args>
  struct pack_args { };

  template<class R, class... Args>
  struct pack {
    using args = pack_args<Args...>;
    using return_type = R;
  };

  class rpc {

    class remote_call_base {
      public:
        virtual ~remote_call_base() {};
    };

    template <typename F>
    class remote_call : public remote_call_base {
      // using function_type = T;
      public:
        virtual ~remote_call() {}

        typedef F listener_t;

        void add_listener(cvm::topology::loc_t loc, F f) {
          if (listeners_.contains(loc)) {
            assert(false && "listener already exists");
            return;
          }

          printf("location %d\n", loc);

          listeners_[loc] = f;
        }

        auto run(cvm::topology::loc_t loc, F::args args) {
          if (!listeners_.contains(loc)) {
            assert(false && "listener does not exist");
          }

          return listeners_[loc](args);
        }

      private:
        std::unordered_map<cvm::topology::loc_t, listener_t> listeners_;
    };

    public:
      template<typename A, typename F>
      void connect(cvm::topology::loc_t loc, std::string funct_name, F l) {
        // using args = A::args;
        // using return_type = A::return_type;

        // using args = pack<T>;
        if (loc == cvm::topology::null) {
          assert(false && "attempting to connect to null location");
        }

        // using A = cvm::pack<R(Args...)>;

        remote_calls<A, F>(funct_name)->add_listener(loc, l);


        cvm::log(cvm::DEBUG, "[remote_procedure_call] connecting listener, location {}, funct_name {}\n", loc, funct_name);
      }

      template<typename A, typename... Args>
      auto signal(cvm::topology::loc_t loc, std::string funct_name, Args... args) {
        if (loc == cvm::topology::null) {
          assert(false && "attempting to signal to a null location");
        }

        cvm::log(cvm::DEBUG, "[remote_procedure_call] signaling listener, location {}, funct_name {}\n", loc, funct_name);
        auto rc = std::dynamic_pointer_cast<remote_call>(remote_calls_.find(funct_name));
        return rc.run(loc, args...);    // TODO: need std::move for loc and args? 
      }

      // void clear();
      // void build();

    private:

      template <typename A, typename F>
      std::shared_ptr<remote_call<F>> remote_calls(std::string funct_name) {
        std::lock_guard<std::mutex> guard(remote_calls_mutex_);
        auto pog = std::type_index(typeid(A));
        temp_map_thing_.emplace(funct_name, pog);
        auto it = remote_calls_.find(funct_name);
        if (it == remote_calls_.end()) {
          std::shared_ptr<remote_call<F>> rc = std::make_shared<remote_call<F>>();
          it = remote_calls_.emplace(funct_name, std::move(rc)).first;
        }
        return std::dynamic_pointer_cast<remote_call<F>>(it->second);
      }

      std::unordered_map<std::string, std::type_index> temp_map_thing_;
      std::unordered_map<std::string, std::shared_ptr<remote_call_base>> remote_calls_;
      std::mutex remote_calls_mutex_;
      std::atomic_flag quit_ = ATOMIC_FLAG_INIT;


  };
}