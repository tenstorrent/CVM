#pragma once

#include <unordered_map>
#include <vector>
#include <functional>
#include <typeinfo>
#include <typeindex>
#include <coroutine>
#include <optional>
#include <memory>
#include <queue>
#include <cassert>
#include <iostream>
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

                      pool(std::vector<task<void>>& handlers) : tasks_(handlers) {};

                      typedef std::function<void(const T&)> listener;
                      void add_long_running(cvm::topology::loc_t loc, const listener& handle) {
                          long_runnings_[loc].emplace_back(handle);
                      }

                      void add_consumer(cvm::topology::loc_t loc, std::coroutine_handle<> handle, T* t = nullptr) {
                          consumers_[loc].emplace_back(handle, t);
                      }

                      void run(cvm::topology::loc_t loc, T t) {
                          // first append to all existing channels
                          for (auto& channel : channels_)
                              if (std::get<0>(channel) == loc)
                                  std::get<1>(channel).emplace(t);

                          const auto& connected = long_runnings_[loc];
                          std::for_each(connected.begin(), connected.end(), [&t] (const auto& handler) { handler(t); });

                          bool clean = false;
                          auto our_consumers = consumers_[loc];
                          consumers_[loc].clear();
                          std::for_each(our_consumers.begin(), our_consumers.end(),
                              [&t, &clean] (auto& consumer) {
                                      if (std::get<1>(consumer))
                                          *(std::get<1>(consumer)) = t;

                                      std::get<0>(consumer).resume();
                                      clean |= std::get<0>(consumer).done();
                              });

                          // not necessary all the time - need to use a GC?
                          if (clean)
                              tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(),
                                  [] (const auto& handler) { return handler.done(); }), tasks_.end());
                      }

                      task<T> wait(cvm::topology::loc_t loc) {
                          struct awaiter {
                              pool<T>& self;
                              cvm::topology::loc_t loc;
                              T t;
                              bool await_ready() { return false; };
                              void await_suspend(std::coroutine_handle<> awaiting) noexcept { self.add_consumer(loc, awaiting, &t); }
                              T await_resume() { return t; };
                          };

                          T t = co_await awaiter{*this, loc, {}};
                          co_return t;
                      }

                      // messenger managed channel
                      struct channel_info{ size_t id; };
                      auto create_channel(cvm::topology::loc_t loc) {
                          channels_.emplace_back(loc, std::queue<T>{});
                          return channel_info{channels_.size() - 1};
                      }

                      task<T> wait(channel_info info) {
                          struct awaiter {
                              pool<T>& self;
                              size_t id;

                              bool await_ready() { return !std::get<1>(self.channels_[id]).empty(); }
                              void await_suspend(std::coroutine_handle<> awaiting) noexcept { self.add_consumer(std::get<0>(self.channels_[id]), awaiting); }
                              T await_resume() noexcept {
                                  auto& q = std::get<1>(self.channels_[id]);
                                  auto val = q.front();
                                  q.pop();
                                  return val;
                              };
                          };

                          if (info.id >= channels_.size())
                              assert(false && "channel id is invalid");
                          else {
                              T t = co_await awaiter{*this, info.id};
                              co_return t;
                          }
                      }

                      void delete_channel(channel_info info) {
                          channels_.erase(channels_.begin() + info.id);
                          return;
                      }

                  private:

                      std::vector<task<void>>& tasks_;

                      typedef std::vector<listener> long_runnings;
                      std::unordered_map<cvm::topology::loc_t, long_runnings> long_runnings_;

                      typedef std::tuple<std::coroutine_handle<>, T*> consumer;
                      std::unordered_map<cvm::topology::loc_t, std::vector<consumer>> consumers_;

                      typedef std::tuple<cvm::topology::loc_t, std::queue<T>> channel;
                      std::vector<channel> channels_;
              };

              template <typename T>
              void connect(cvm::topology::loc_t loc, const typename pool<T>::listener& l) {
                  if (loc == cvm::topology::null) {
                      assert(false && "attempting to connect to null location");
                      return;
                  }
                  message_pool<T>().add_long_running(loc, l);
                  return;
              }

              template <typename U, typename... Args>
              requires std::invocable<U, Args...>
              void fork(U l, Args&&... args) {
                  auto forked = (*l)(std::forward<Args>(args)...);
                  forked.resume();
                  if (!forked.done())
                      tasks_.emplace_back(std::move(forked));
                  return;
              }

              // We take transaction by value, because coroutine may outlive reference
              template <typename T>
              void signal(cvm::topology::loc_t loc, const T t) {
                  if (loc == cvm::topology::null) {
                      assert(false && "attempting to signal to null location");
                      return;
                  }
                  message_pool<T>().run(loc, std::move(t));
                  return;
              }

              template <typename T>
              task<T> wait(cvm::topology::loc_t loc) {
                  T t = co_await message_pool<T>().wait(loc);
                  co_return t;
              }

              template <typename T>
              task<T> wait(typename pool<T>::channel_info info) {
                  T t = co_await message_pool<T>().wait(info);
                  co_return t;
              }

              template <typename T>
              auto channel(cvm::topology::loc_t loc) {
                  return message_pool<T>().create_channel(loc);
              }

              template <typename T>
              void del(typename pool<T>::channel_info info) {
                  message_pool<T>().delete_channel(info);
                  return;
              }

              void clear() {
                  tasks_.clear();
                  pools_.clear();
              }

          private:

              template <typename T>
              auto& message_pool() {
                  auto key = std::type_index(typeid(T));
                  if (pools_.find(key) == pools_.end())
                      pools_[key] = std::unique_ptr<pool_base>(new pool<T>(tasks_));
                  return *(dynamic_cast<pool<T>*>(pools_[key].get()));
              }

              std::vector<task<void>> tasks_;
              std::unordered_map<std::type_index, std::unique_ptr<pool_base>> pools_;
      };
}
