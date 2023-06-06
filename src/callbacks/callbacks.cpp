#include "cvm/callbacks.hpp"
#include "cvm/plusargs.hpp"
#include <chrono>
using namespace std::chrono_literals;

DEFINE_bool(cb_async, false, "use asynchronous callbacks");

using namespace cvm;

callbackss::callbackss() {
}

callbackss::~callbackss() {
  quit_ = true;
  if (async_.joinable())
    async_.join();
}

void
callbackss::flush() {
  std::unique_lock<std::mutex> lock(m_);
  while (FLAGS_cb_async && que_.empty()) {
    c_.wait_for(lock, 100ms);
    if (this->finished()) return;
  }
  std::for_each(que_.begin(), que_.end(), [](scoped_cb cb) { svSetScope(std::get<0>(cb)); std::get<1>(cb)(); });
  que_.clear();
}

void
callbackss::clear() {
  std::lock_guard<std::mutex> lock(m_);

  // if async, will spawn a separate thread to issue callbacks
  quit_ = true;
  if (async_.joinable())
    async_.join();

  quit_ = false;
  if (FLAGS_cb_async) {
    async_ = std::thread([&] () {
      while(not this->finished()) { this->flush(); }});
  }

  que_.clear();
}
