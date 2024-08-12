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

  class rpc {

    class remote_call_base {
      public:
        virtual ~remote_call_base() {};
    };

    template <typename R, typename... Args>
    class remote_call : public remote_call_base {
      public:
        virtual ~remote_call() {}

        typedef std::function<R (Args... args)> listener_t;

        void add_listener(cvm::topology::loc_t loc, const listener_t& handle) {
          if (listeners_.contains(loc)) {
            assert(false && "listener already exists");
            return;
          }

          listeners_[loc] = handle;
        }

        R run(cvm::topology::loc_t loc, Args... args) {
          if (!listeners_.contains(loc)) {
            assert(false && "listener does not exist");
          }

          return listeners_[loc](args...);
        }

      private:
        std::unordered_map<cvm::topology::loc_t, listener_t> listeners_;
    };

    public:
      template<typename R, typename... Args>
      void connect(cvm::topology::loc_t loc, std::string funct_name, const typename remote_call<R, Args...>::listener_t& l) {
        if (loc == cvm::topology::null) {
          assert(false && "attempting to connect to null location");
        }

        remote_calls<R, Args...>(funct_name)->add_listener(loc, l);

        cvm::log(cvm::DEBUG, "[remote_procedure_call] connecting listener, location {}, funct_name {}\n", loc, funct_name);
      }

      template<typename R, typename... Args>
      R signal(cvm::topology::loc_t loc, std::string funct_name, Args... args) {
        if (loc == cvm::topology::null) {
          assert(false && "attempting to signal to a null location");
        }

        cvm::log(cvm::DEBUG, "[remote_procedure_call] signaling listener, location {}, funct_name {}\n", loc, funct_name);
        return remote_calls<R, Args...>(funct_name)->run(loc, args...);    // TODO: need std::move for loc and args? 
      }

      // void clear();
      // void build();

    private:

      template <typename R, typename... Args>
      std::shared_ptr<remote_call<R, Args...>> remote_calls(std::string funct_name) {
        std::lock_guard<std::mutex> guard(remote_calls_mutex_);
        auto it = remote_calls_.find(funct_name);
        if (it == remote_calls_.end()) {
          std::shared_ptr<remote_call<R, Args...>> rc = std::make_shared<remote_call<R, Args...>>();
          it = remote_calls_.emplace(funct_name, std::move(rc)).first;
        }
        return std::dynamic_pointer_cast<remote_call<R, Args...>>(it->second);
      }

      std::unordered_map<std::string, std::shared_ptr<remote_call_base>> remote_calls_;
      std::mutex remote_calls_mutex_;
      std::atomic_flag quit_ = ATOMIC_FLAG_INIT;


  };
}