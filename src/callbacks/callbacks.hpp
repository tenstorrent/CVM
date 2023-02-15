#pragma once

#include <mutex>
#include <condition_variable>
#include <functional>
#include <list>
#include "svdpi.h"

namespace cvm {

  // unified way for modules to add callbacks from C/C++ land
  // module should add a void function to queue
  class cb_que {
    public:

      cb_que();

      typedef std::function<void()> cb;
      void push(svScope scope, const std::string& tag, const cb& func);

      /// blocking pull
      void pull();

      void flush(const std::string& tag);

      void flush();

    private:

      std::condition_variable c_;
      mutable std::mutex m_;

      typedef std::tuple<svScope, std::string, cb> scoped_cb;
      std::list<scoped_cb> que_;
  };

  class callbacks {

    inline static cb_que que_;

    static void push(svScope scope, const std::string& tag, const cb_que::cb& func)
    {
      que_.push(scope, tag, func);
    }

    static void pull()
    {
      que_.pull();
    }

    /// non-blocking flush (for polling)
    /// will only flush if tag matches
    static void flush(const std::string& tag)
    {
      que_.flush(tag);
    }
  };
}
