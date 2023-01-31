// unified way for modules to add callbacks from C/C++ land
// module should add a void function to queue
#pragma once

#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>
#include "svdpi.h"

namespace cvm {

  namespace callbacks {
    typedef std::function<void()> cb;
    void push(svScope scope, const cb& func);

    void pull();

    void flush();

    class CbQue {
      public:

        /// if fast, will spawn a separate thread to issue callbacks
        CbQue(bool fast);

        void push(svScope scope, const cb& func);

        /// blocking pull
        void pull();

        /// non-blocking flush
        void flush();

      private:

        std::condition_variable c_;
        mutable std::mutex m_;

        typedef std::pair<svScope, const cb> scoped_cb;
        std::queue<scoped_cb> que_;
    };
  }
}
