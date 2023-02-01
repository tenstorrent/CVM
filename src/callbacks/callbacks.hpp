#pragma once

#include <mutex>
#include <condition_variable>
#include <functional>
#include <list>
#include "svdpi.h"

namespace cvm {

  namespace callbacks {

    typedef std::function<void()> cb;
    void push(svScope scope, const std::string& tag, const cb& func);

    void pull();

    /// non-blocking flush (for polling)
    /// will only flush if tag matches
    void flush(const std::string& tag);

    // unified way for modules to add callbacks from C/C++ land
    // module should add a void function to queue

    class CbQue {
      public:

        /// if fast, will spawn a separate thread to issue callbacks
        CbQue(bool fast);

        void push(svScope scope, const std::string& tag, const cb& func);

        /// blocking pull
        void pull();

        void flush(const std::string& tag);

        void flush();

      private:

        std::condition_variable c_;
        mutable std::mutex m_;

        typedef std::tuple<svScope, std::string, const cb> scoped_cb;
        std::list<scoped_cb> que_;
    };
  }
}
