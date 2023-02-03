#pragma once

#include <mutex>
#include <condition_variable>
#include <functional>
#include <list>
#include "svdpi.h"

namespace cvm {

  struct callbacks {

    typedef std::function<void()> cb;
    static void push(svScope scope, const std::string& tag, const cb& func);

    static void pull();

    /// non-blocking flush (for polling)
    /// will only flush if tag matches
    static void flush(const std::string& tag);
  };

  // unified way for modules to add callbacks from C/C++ land
  // module should add a void function to queue
  class CbQue {
    public:

      CbQue();

      void push(svScope scope, const std::string& tag, const callbacks::cb& func);

      /// blocking pull
      void pull();

      void flush(const std::string& tag);

      void flush();

    private:

      std::condition_variable c_;
      mutable std::mutex m_;

      typedef std::tuple<svScope, std::string, const callbacks::cb> scoped_cb;
      std::list<scoped_cb> que_;
  };
}
