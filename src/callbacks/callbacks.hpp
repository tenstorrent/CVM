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
  class callbackss {
    public:

      callbackss();

      ~callbackss();

      typedef std::function<void()> cb;
      void push(svScope scope, const cb& func);

      /// blocking pull
      void pull();

      void flush();

      void clear();

      inline bool finished()
      { return quit_ == true; }

    private:

      std::condition_variable c_;
      mutable std::mutex m_;

      typedef std::tuple<svScope, cb> scoped_cb;
      std::queue<scoped_cb> que_;

      std::thread async_;
      std::atomic<bool> quit_ = false;
  };
}
