#include "cvm/callbacks.hpp"
#include "cvm/plusargs.hpp"
#include <thread>

DEFINE_bool(cb_async, false, "use asynchronous callbacks");

using namespace cvm::callbacks;
static CbQue queue(FLAGS_cb_async);

void
push(svScope scope, const std::string& tag, const cb& func) {
  queue.push(scope, tag, func);
}

void
pull() {
  queue.pull();
}

void
flush(const std::string& tag) {
  queue.flush(tag);
}

CbQue::CbQue(bool fast) {
  if (fast) {
    std::thread([&] () {
      while(1) { this->pull(); }}).detach();
  }
}

void
CbQue::push(svScope scope, const std::string& tag, const cb& func) {
  std::lock_guard<std::mutex> lock(m_);
  que_.emplace_back(std::make_tuple(scope, tag, func));
  c_.notify_one();
}

void
CbQue::pull() {
  std::unique_lock<std::mutex> lock(m_);
  while (que_.empty()) {
    c_.wait(lock);
  }
  auto [scope, tag, func] = std::move(que_.front());
  que_.pop_front();
  svSetScope(scope);
  func();
}

void
CbQue::flush(const std::string& tag) {
  std::lock_guard<std::mutex> lock(m_);
  auto it = que_.begin();
  while (it != que_.end()) {
    auto [scope, used, func] = *it;
    if (used == tag) {
      it = que_.erase(it);
      svSetScope(scope);
      func();
    }
    else {
      ++it;
    }
  }
}

void
CbQue::flush() {
  std::lock_guard<std::mutex> lock(m_);
  for (const auto& [scope, tag, func] : que_) {
    svSetScope(scope);
    func();
  }
  que_.clear();
}
