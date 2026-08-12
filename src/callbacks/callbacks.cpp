// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/callbacks.hpp"
#include "cvm/plusargs.hpp"
#include <chrono>
using namespace std::chrono_literals;

DEFINE_bool(cb_async, false, "use asynchronous callbacks");
DEFINE_uint64(cb_async_join_timeout_ms, 5000, "time to wait for async thread to join");

using namespace cvm;

callbacks::callbacks() {
}

callbacks::~callbacks() {
  while (!clear());
}

void
callbacks::flush() {

  std::unique_lock<std::timed_mutex> flush_lock(flush_mutex_);

  while(1) {
    cb func;
    {
      // zebu can deadlock if we keep the mutex while calling the DPI export
      // zebu may wait for any DPI imports to return before allowing DPI exports to proceed
      // so a DPI export call could be blocked by zebu waiting on a DPI import to finish, meanwhile the DPI import is blocked on this mutex
      std::unique_lock<std::mutex> lock(m_);

      while (que_.empty()) {
        if (!FLAGS_cb_async || this->finished()) return;
        c_.wait_for(lock, 100ms);
      }

      func = std::move(que_.front());
      que_.erase(que_.begin());
    }
    func();
  }

}

void
callbacks::build() {
  quit_ = false;
  if (FLAGS_cb_async) {
    async_ = std::thread(std::bind(&callbacks::flush, this));
  }
}

bool
callbacks::clear() {
  // if async, will spawn a separate thread to issue callbacks
  quit_ = true;
  if (async_.joinable()) {
    std::unique_lock<std::timed_mutex> lock(flush_mutex_, 1ms * FLAGS_cb_async_join_timeout_ms);
    if (!lock) {
      return false;
    }
    async_.join();
  }

  {
    std::lock_guard<std::mutex> lock(m_);
    que_.clear();
  }

  // {
  //   std::lock_guard<std::mutex> lock(scope_m_);
  //   sv_scopes_.clear();
  // }

  return true;
}

void
callbacks::set_scope(cvm::topology::loc_t loc, svScope scope) {
  std::lock_guard<std::mutex> lock(scope_m_);
  auto it = sv_scopes_.find(loc);
  if (it != sv_scopes_.end() && it->second != scope) {
    cvm::log(cvm::ERROR, "Error: callbacks::set_scope: loc {} already registered with a different scope\n", loc);
    return;
  }
  sv_scopes_[loc] = scope;
}


