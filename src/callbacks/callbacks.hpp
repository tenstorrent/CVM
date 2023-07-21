#pragma once

#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>
#include <thread>
#include <atomic>
#include "svdpi.h"

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
        std::lock_guard<std::mutex> lock(m_);
        que_.emplace_back(scope, std::forward<T>(func));
        c_.notify_one();
      }

      void flush();

      void build();

      void clear();

      inline bool finished()
      { return quit_ == true; }

    private:

      std::condition_variable c_;
      std::mutex m_;

      typedef std::tuple<svScope, cb> scoped_cb;
      std::vector<scoped_cb> que_;

      std::thread async_;
      std::atomic<bool> quit_ = false;
  };
}
