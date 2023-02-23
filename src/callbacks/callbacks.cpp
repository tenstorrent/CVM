#include "cvm/callbacks.hpp"
#include "cvm/plusargs.hpp"

DEFINE_bool(cb_async, false, "use asynchronous callbacks");

using namespace cvm;

callbackss::callbackss() {
  // if async, will spawn a separate thread to issue callbacks
  if (FLAGS_cb_async) {
    async_ = std::thread([&] () {
      while(not this->finished()) { this->pull(); }});
  }
}

callbackss::~callbackss() {
  quit_ = true;
  if (async_.joinable())
    async_.join();
}

void
callbackss::push(svScope scope, const cb& func) {
  std::lock_guard<std::mutex> lock(m_);
  que_.emplace(std::make_tuple(scope, func));
  c_.notify_one();
}

void
callbackss::pull() {
  std::unique_lock<std::mutex> lock(m_);
  while (que_.empty()) {
    c_.wait(lock);
  }
  auto [scope, func] = std::move(que_.front());
  que_.pop();
  svSetScope(scope);
  func();
}

void
callbackss::flush() {
  if (FLAGS_cb_async)
    return;

  std::lock_guard<std::mutex> lock(m_);
  while (!que_.empty()) {
    auto [scope, func] = std::move(que_.front());
    que_.pop();
    svSetScope(scope);
    func();
  }
}

void
callbackss::clear() {
  std::lock_guard<std::mutex> lock(m_);
  while (!que_.empty()) {
    que_.pop();
  }
}
