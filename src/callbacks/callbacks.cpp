#include "cvm/callbacks.hpp"
#include "cvm/plusargs.hpp"
#include <chrono>
using namespace std::chrono_literals;

DEFINE_bool(cb_async, false, "use asynchronous callbacks");

using namespace cvm;

callbacks::callbacks() {
}

callbacks::~callbacks() {
  clear();
}

void
callbacks::flush() {
  while(1) {
    scoped_cb cb;
    {
      // zebu can deadlock if we keep the mutex while calling the DPI export
      // zebu may wait for any DPI imports to return before allowing DPI exports to proceed
      // so a DPI export call could be blocked by zebu waiting on a DPI import to finish, meanwhile the DPI import is blocked on this mutex
      std::unique_lock<std::mutex> lock(m_);

      while (que_.empty()) {
        if (!FLAGS_cb_async || this->finished()) return;
        c_.wait_for(lock, 100ms);
      }

      cb = std::move(que_.front());
      que_.erase(que_.begin());
    }
    svSetScope(std::get<0>(cb));
    std::get<1>(cb)();
  }
}

void
callbacks::build() {
  quit_ = false;
  if (FLAGS_cb_async) {
    async_ = std::thread(std::bind(&callbacks::flush, this));
  }
}

void
callbacks::clear() {
  // if async, will spawn a separate thread to issue callbacks
  quit_ = true;
  if (async_.joinable())
    async_.join();

  {
    std::lock_guard<std::mutex> lock(m_);
    que_.clear();
  }
}
