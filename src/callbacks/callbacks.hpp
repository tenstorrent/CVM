// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>
#include <thread>
#include <atomic>
#include <unordered_map>
#include "svdpi.h"
#include "cvm/topology.hpp"
#include "cvm/logger.hpp"

namespace cvm {

  // unified way for modules to add callbacks from C/C++ land
  // callbacks should be a void function
  class callbacks {
    public:

      callbacks();

      ~callbacks();

      typedef std::function<void()> cb;

      template <typename T,
                typename = std::enable_if<std::is_same<T, cb>::value>>
      void push(svScope scope, T&& func)
      {
        push([this, scope, f = cb(std::forward<T>(func))]() { call(scope, f); });
      }

      void set_scope(cvm::topology::loc_t loc, svScope scope);

      template <typename T,
                typename = std::enable_if<std::is_same<T, cb>::value>>
      void push(cvm::topology::loc_t loc, T&& func)
      {
        push([this, loc, f = cb(std::forward<T>(func))]() { call(loc, f); });
      }

      // Like push, but runs the callback immediately.
      template <typename T,
                typename = std::enable_if<std::is_same<T, cb>::value>>
      void call(cvm::topology::loc_t loc, T&& func)
      {
        svScope s;
        {
          std::lock_guard<std::mutex> lock(scope_m_);
          auto it = sv_scopes_.find(loc);
          if (it == sv_scopes_.end()) {
            cvm::log(cvm::ERROR, "Error: callbacks::call: no svScope registered for loc {}\n", loc);
            return;
          }
          s = it->second;
        }
        call(s, std::forward<T>(func));
      }

      void flush();

      void build();

      bool clear();

      inline bool finished()
      { return quit_ == true; }

    private:

      // The single entry point to the queue.
      void push(cb&& func)
      {
        {
          std::lock_guard<std::mutex> lock(m_);
          que_.emplace_back(std::move(func));
        }
        c_.notify_one();
      }

      // The single exit point to the dpi callbacks.
      template <typename T,
                typename = std::enable_if<std::is_same<T, cb>::value>>
      void call(svScope scope, T&& func)
      {
        svScope prev = svSetScope(scope);
        func();
        if (prev) svSetScope(prev);
      }

      std::condition_variable c_;
      std::mutex m_;

      std::vector<cb> que_;

      std::thread async_;
      std::atomic<bool> quit_ = false;
      std::timed_mutex flush_mutex_;

      std::mutex scope_m_;
      std::unordered_map<cvm::topology::loc_t, svScope> sv_scopes_;
  };
}
