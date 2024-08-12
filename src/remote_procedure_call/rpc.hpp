#pragma once

#include <cassert>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>

// #include "cvm/messenger.hpp"
#include "cvm/topology.hpp"
#include "cvm/logger.hpp"
#include "cvm/type_traits.hpp"


namespace cvm {

  class rpc {
    public:

      // base class that allows for map of pointers to be formed without template
      class remote_call_base {
        public: 
          virtual ~remote_call_base() {};
      };

      // based on messenger pool
      template <typename T, typename R>
      class remote_call : public remote_call_base {
        public:

          virtual ~remote_call() {}

          typedef std::function<R(const T&)> listener_t;
          void add_long_running(cvm::topology::loc_t loc, const listener_t& handle) {
            if (listeners_.contains(loc)) {
              assert(false && "listener already exists");
              return;
            }

            listeners_[loc] = handle;

            if (!listeners_.contains(loc)) {
              assert(false && "listener wasn't added to loc");
            }            
          }

          R run(cvm::topology::loc_t loc, T t) {
            // Called by signal
            // Runs one task vs messenger's multiple connected tasks

            if (!listeners_.contains(loc)) {
              
              // cvm::log(cvm::ERROR, "[remote_procedure_call] listener does not exist, loc {} type {}\n", loc, typeid(T).name());
              assert(false && "listener does not exist");
            }

            auto rpc_listener = listeners_[loc];

            return rpc_listener(t);  // run listener at loc given arg t, return the value to caller
          }

        private:
          // pointer to the remote function
          // listener_t listener = nullptr;
          std::unordered_map<cvm::topology::loc_t, listener_t> listeners_;    // one listener per location
      };

      public:

        template <typename T, typename R>
        void connect(cvm::topology::loc_t loc, const typename remote_call<T, R>::listener_t& l) {
          cvm::log(cvm::DEBUG, "[remote_procedure_call] connect to location {} of type {} with typeid {}\n", loc, typeid(T).name(), std::type_index(typeid(T)).hash_code());
          if (loc == cvm::topology::null) {
            assert(false && "attempting to connect to null location");
            return;
          }

          remote_calls<T, R>()->add_long_running(loc, l);

        }

        template <typename T, typename R>
        R signal(cvm::topology::loc_t loc, const T& m) {
          cvm::log(cvm::DEBUG, "[remote_procedure_call] signal to location {} of type {} with typeid {}\n", loc, typeid(T).name(), std::type_index(typeid(T)).hash_code());
          if (loc == cvm::topology::null) {
            assert(false && "attempting to signal to null location");
          }

          return remote_calls<T, R>()->run(std::move(loc), std::move(m));
        }


        // empty the remote_calls_ map
        void clear();
        void build();
        

      
      private:

        template <typename T, typename R>
        std::shared_ptr<remote_call<T, R>> remote_calls() {
          auto key = std::type_index(typeid(T));
          std::lock_guard<std::mutex> guard(remote_calls_mutex_);
          auto it = remote_calls_.find(key);
          if (it == remote_calls_.end()) {
            std::shared_ptr<remote_call<T, R>> rc = std::make_shared<remote_call<T, R>>();
            it = remote_calls_.emplace(key, std::move(rc)).first;
          }
          return std::dynamic_pointer_cast<remote_call<T, R>>(it->second);

        }

        // double map for type T and R
        // TODO: is there a way to combine T and R into a single type index? maybe pair but would need to define a hash function I think
        // std::unordered_map<std::type_index, std::unordered_map<std::type_index, std::shared_ptr<remote_call<T, R>>> remote_calls_;
        // Actually... its safe to assume each T will have one and only one R to go with it
        std::unordered_map<std::type_index, std::shared_ptr<remote_call_base>> remote_calls_;
        std::mutex remote_calls_mutex_;
        std::atomic_flag quit_ = ATOMIC_FLAG_INIT;

  };
}