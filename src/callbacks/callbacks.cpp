#include "cvm/callbacks.hpp"
#include "cvm/plusargs.hpp"
#include <thread>

DEFINE_bool(cb_fast, false, "use asynchronous callbacks");

using namespace cvm::callbacks;
static CbQue queue(FLAGS_cb_fast);

void
push(svScope scope, const cb& func) {
  queue.push(scope, func);
}

void
pull() {
  queue.pull();
}

void
flush() {
  queue.flush();
}

CbQue::CbQue(bool fast) {
  if (fast) {
    std::thread([&] () {
      while(1) { this->pull(); }}).detach();
  }
}

void
CbQue::push(svScope scope, const cb& func) {
  std::lock_guard<std::mutex> lock(m_);
  que_.push(std::make_pair(scope, func));
  c_.notify_one();
}

void
CbQue::pull() {
  std::unique_lock<std::mutex> lock(m_);
  while (que_.empty()) {
    c_.wait(lock);
  }
  auto scoped = std::move(que_.front());
  que_.pop();
  svSetScope(scoped.first);
  scoped.second();
}

void
CbQue::flush() {
  std::lock_guard<std::mutex> lock(m_);
  while (!que_.empty()) {
    auto scoped = std::move(que_.front());
    que_.pop();
    svSetScope(scoped.first);
    scoped.second();
  }
}
