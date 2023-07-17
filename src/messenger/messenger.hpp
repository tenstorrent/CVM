#pragma once

#include <unordered_map>
#include <vector>
#include <functional>
#include <typeinfo>
#include <typeindex>
#include <coroutine>
#include <optional>
#include <memory>
#include <deque>
#include <cassert>
#include <thread>
#include <mutex>
#include <iostream>
#include <ranges>
#include "cvm/topology.hpp"

namespace cvm {

      class messenger {

          public:

              template <typename T = void>
              class task {
                  public:
                      struct promise_type {
                          std::optional<T> value_;
                          std::coroutine_handle<> awaiting_;

                          task get_return_object() noexcept { return task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
                          std::suspend_always initial_suspend() noexcept { return {}; }
                          void return_value(T val) noexcept { value_ = val; }
                          void unhandled_exception() { std::terminate(); }

                          // handler is done, resume awaiting coroutine
                          // https://lewissbaker.github.io/2020/05/11/understanding_symmetric_transfer
                          auto final_suspend() noexcept {
                              struct awaiter {
                                  promise_type& self;

                                  bool await_ready() noexcept { return false; }
                                  std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
                                      return (self.awaiting_) ? self.awaiting_ : std::noop_coroutine();
                                  }
                                  void await_resume() noexcept { }
                              };

                              return awaiter{*this};
                          }
                      };

                      // possible to co_await on handler
                      auto operator co_await() & noexcept {
                          struct awaiter {
                              std::coroutine_handle<promise_type>& coro_;
                              bool await_ready() noexcept { return false; };
                              std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) noexcept {
                                  coro_.promise().awaiting_ = awaiting;
                                  return coro_;
                              }
                              auto await_resume() noexcept { return std::move(*(coro_.promise().value_)); coro_.destroy(); coro_ = nullptr; };
                          };
                          return awaiter{this->coro_};
                      }

                      auto operator co_await() && noexcept {
                          struct awaiter {
                              std::coroutine_handle<promise_type>& coro_;
                              bool await_ready() { return false; };
                              std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) noexcept {
                                  coro_.promise().awaiting_ = awaiting;
                                  return coro_;
                              }
                              auto await_resume() noexcept { return std::move(*(coro_.promise().value_)); coro_.destroy(); coro_ = nullptr; };
                          };
                          return awaiter{this->coro_};
                      }

                      explicit task(std::coroutine_handle<promise_type> coro) : coro_(coro) {}

                      task(task&& other) noexcept : coro_(std::exchange(other.coro_, nullptr)) {}
                      task& operator=(task&& other) noexcept {
                          if (std::addressof(other) != this) {
                              if (coro_) coro_.destroy();
                              coro_ = other.coro_;
                              other.coro_ = nullptr;
                          }

                          return *this;
                      }

                      task(const task&) = delete;
                      task& operator=(const task&) = delete;

                      ~task() { if (coro_) coro_.destroy(); }

                      bool done() const { if (coro_) return coro_.done(); else return true; }
                      void resume() { if(coro_) coro_.resume(); }

                      std::coroutine_handle<promise_type> coro_;
              };

              template <>
              class task<void> {
                  public:
                      struct promise_type {
                          std::coroutine_handle<> awaiting_;

                          task get_return_object() noexcept { return task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
                          std::suspend_always initial_suspend() noexcept { return {}; }
                          void return_void() noexcept { return; }
                          void unhandled_exception() { std::terminate(); }

                          // handler is done, resume awaiting coroutine
                          // https://lewissbaker.github.io/2020/05/11/understanding_symmetric_transfer
                          auto final_suspend() noexcept {
                              struct awaiter {
                                  promise_type& self;

                                  bool await_ready() noexcept { return false; }
                                  std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
                                      return (self.awaiting_) ? self.awaiting_ : std::noop_coroutine();
                                  }
                                  void await_resume() noexcept {}
                              };

                              return awaiter{*this};
                          }
                      };

                      // possible to co_await on handler
                      auto operator co_await() & noexcept {
                          struct awaiter {
                              std::coroutine_handle<promise_type>& coro_;
                              bool await_ready() noexcept { return false; };
                              std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) noexcept {
                                  coro_.promise().awaiting_ = awaiting;
                                  return coro_;
                              }
                              void await_resume() noexcept { coro_.destroy(); coro_ = nullptr; };
                          };
                          return awaiter{this->coro_};
                      }

                      auto operator co_await() && noexcept {
                          struct awaiter {
                              std::coroutine_handle<promise_type>& coro_;
                              bool await_ready() { return false; };
                              std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) noexcept {
                                  coro_.promise().awaiting_ = awaiting;
                                  return coro_;
                              }
                              void await_resume() noexcept { coro_.destroy(); coro_ = nullptr; };
                          };
                          return awaiter{this->coro_};
                      }

                      explicit task(std::coroutine_handle<promise_type> coro) : coro_(coro) {}

                      task(task&& other) noexcept : coro_(std::exchange(other.coro_, nullptr)) {}
                      task& operator=(task&& other) noexcept {
                          if (std::addressof(other) != this) {
                              if (coro_) coro_.destroy();
                              coro_ = other.coro_;
                              other.coro_ = nullptr;
                          }

                          return *this;
                      }

                      task(const task&) = delete;
                      task& operator=(const task&) = delete;

                      ~task() { if (coro_) coro_.destroy(); }

                      bool done() const { if (coro_) return coro_.done(); else return true; }
                      void resume() { if(coro_) coro_.resume(); }

                      std::coroutine_handle<promise_type> coro_;
              };


              class pool_base {
                  public:
                    virtual ~pool_base() {};
              };

              template <typename T>
              class pool : public pool_base {
                  public:

                      virtual ~pool() {}

                      typedef std::function<void(const T&)> listener;
                      void add_long_running(cvm::topology::loc_t loc, const listener& handle, const std::function<bool(const T&)>& filter) {
                          long_runnings_[loc].emplace_back(handle, filter);
                      }

                      bool run(cvm::topology::loc_t loc, T t) {
                          std::vector<std::coroutine_handle<>> handles;

                          // first append to all existing channels and moments. register handles which need to be resumed
                          std::for_each(moments_[loc].begin(), moments_[loc].end(),
                              [&t, &handles] (auto& moment) {
                                  *(moment.val) = t;
                                  assert(moment.handle && "moment waiting on null coroutine");
                                  handles.emplace_back(std::move(moment.handle));
                              });

                          moments_[loc].clear();

                          std::for_each(channels_[loc].begin(), channels_[loc].end(),
                              [&t, &handles] (auto& channel) {
                                  // we only proceed if t passes filter
                                  if (!(channel.filter) || (channel.filter)(t)) {
                                      channel.vals.emplace_back(t);
                                      if (channel.handle) {
                                          handles.emplace_back(std::move(channel.handle));
                                          channel.handle = nullptr;
                                      }
                                  }
                              });

                          // resume awaiting tasks and listeners
                          bool clean = false;
                          std::for_each(handles.begin(), handles.end(),
                              [&clean] (const auto& handle) {
                                  handle.resume();
                                  clean |= handle.done();
                              });

                          auto& connected = long_runnings_[loc];
                          std::for_each(connected.begin(), connected.end(),
                              [&t] (auto& handle) {
                                  if (!(handle.filter) || (handle.filter)(t)) {
                                      (handle.l)(t);
                                  }
                              });

                          return clean;
                      }

                      task<T> wait(cvm::topology::loc_t loc) {
                          struct awaiter {
                              pool<T>& self;
                              cvm::topology::loc_t loc;

                              T t;
                              size_t id;

                              bool await_ready() noexcept { return false; };
                              void await_suspend(std::coroutine_handle<> awaiting) noexcept { self.moments_[loc].emplace_back(&t, awaiting); };
                              T await_resume() noexcept { return t; };
                          };

                          co_return co_await awaiter{*this, loc};
                      }

                      // messenger managed channel
                      struct channel_info{ size_t id; cvm::topology::loc_t loc; };
                      auto create_channel(cvm::topology::loc_t loc, const std::function<bool(const T&)>& filter) {
                          channels_[loc].emplace_back(nullptr, filter);
                          return channel_info{channels_[loc].size() - 1, loc};
                      }

                      void update_channel_filter(channel_info info, const std::function<bool(const T& t)>& filter) {
                          if (info.id >= channels_[info.loc].size())
                              assert(false && "channel id is invalid");
                          else
                              channels_[info.loc][info.id].filter = filter;
                      }

                      task<T> wait(channel_info info) {
                          struct awaiter {
                              pool<T>& self;
                              channel_info info;

                              bool await_ready() noexcept {
                                  // we can support this if we store a vector of awaiters instead
                                  // slow?
                                  auto& awaiter = self.channels_[info.loc][info.id].handle;
                                  assert(!awaiter && "multiple tasks waiting on same channel");
                                  auto& channel = self.channels_[info.loc][info.id].vals;
                                  return !channel.empty();
                              };
                              void await_suspend(std::coroutine_handle<> awaiting) noexcept {
                                  self.channels_[info.loc][info.id].handle = awaiting;
                              };
                              T await_resume() noexcept {
                                  auto& q = self.channels_[info.loc][info.id].vals;
                                  auto val = std::move(q.front()); q.pop_front();
                                  return val;
                              };
                          };

                          if (info.id >= channels_[info.loc].size())
                              assert(false && "channel id is invalid");
                          else
                              co_return co_await awaiter{*this, info};
                      }

                      void delete_channel(channel_info info) {
                          channels_[info.loc].erase(channels_.begin() + info.id);
                          return;
                      }

                  private:

                      struct long_running {
                          long_running(listener l, std::function<bool(const T&)> filter) : l(l), filter(filter) {};
                          listener l;
                          std::function<bool(const T&)> filter;
                      };
                      std::unordered_map<cvm::topology::loc_t, std::vector<long_running>> long_runnings_;

                      struct moment {
                          moment(T* val, std::coroutine_handle<> handle) : val(val), handle(handle) {};
                          T* val;
                          std::coroutine_handle<> handle;
                      };
                      std::unordered_map<cvm::topology::loc_t, std::vector<moment>> moments_;

                      struct channel {
                          channel(std::coroutine_handle<> handle, std::function<bool(const T&)> filter) : handle(handle), filter(filter) {};
                          std::deque<T> vals;
                          std::coroutine_handle<> handle;
                          std::function<bool(const T&)> filter;
                      };
                      std::unordered_map<cvm::topology::loc_t, std::vector<channel>> channels_;
              };

              // TODO: use variadic templates
              template <typename T>
              void connect(cvm::topology::loc_t loc, const typename pool<T>::listener& l, const std::function<bool(const T& t)>& filter = nullptr) {
                  if (loc == cvm::topology::null) {
                      assert(false && "attempting to connect to null location");
                      return;
                  }
                  message_pool<T>()->add_long_running(loc, l, filter);
                  return;
              }

              template <typename U, typename... Args>
              requires std::invocable<U, Args...>
              void fork(U l, Args&&... args) {
                  auto forked = (*l)(std::forward<Args>(args)...);
                  forked.resume();
                  if (!forked.done()) {
                      std::lock_guard<std::mutex> guard(tasks_mutex_);
                      tasks_.emplace_back(std::move(forked));
                  }
                  return;
              }

              template <typename T>
              void signal(cvm::topology::loc_t loc, const T& t) {
                  if (loc == cvm::topology::null) {
                      assert(false && "attempting to signal to null location");
                      return;
                  }
                  bool clean = message_pool<T>()->run(loc, std::move(t));

                  // not necessary all the time - need to use a GC?
                  if (clean) {
                      std::lock_guard<std::mutex> guard(tasks_mutex_);
                      tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(),
                          [] (const auto& handler) { return handler.done(); }), tasks_.end());
                  }

                  return;
              }

              template <typename T>
              task<T> wait(cvm::topology::loc_t loc) {
                  co_return co_await message_pool<T>()->wait(loc);
              }

              template <typename T>
              task<T> wait(typename pool<T>::channel_info info) {
                  co_return co_await message_pool<T>()->wait(info);
              }

              template <typename T>
              auto channel_filter(typename pool<T>::channel_info info, const std::function<bool(const T& t)>& filter) {
                  return message_pool<T>()->update_channel_filter(info, filter);
              }

              template <typename T>
              auto channel(cvm::topology::loc_t loc, const std::function<bool(const T& t)>& filter = nullptr) {
                  return message_pool<T>()->create_channel(loc, filter);
              }

              template <typename T>
              void del(typename pool<T>::channel_info info) {
                  message_pool<T>()->delete_channel(info);
                  return;
              }

              void build() {}

              void clear() {
                  {
                      std::lock_guard<std::mutex> tasks_guard(tasks_mutex_);
                      tasks_.clear();
                  }
                  {
                      std::lock_guard<std::mutex> pools_guard(pools_mutex_);
                      pools_.clear();
                  }
              }

          private:

              template <typename T>
              std::shared_ptr<pool<T>> message_pool() {
                  auto key = std::type_index(typeid(T));
                  std::lock_guard<std::mutex> guard(pools_mutex_);
                  auto it = pools_.find(key);
                  if (it == pools_.end()) {
                      std::shared_ptr<pool_base> p(new pool<T>());
                      it = pools_.emplace(key, std::move(p)).first;
                  }
                  return std::dynamic_pointer_cast<pool<T>>(it->second);
              }

              std::vector<task<void>> tasks_;
              std::mutex tasks_mutex_;
              std::unordered_map<std::type_index, std::shared_ptr<pool_base>> pools_;
              std::mutex pools_mutex_;
      };
}
