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
#include <condition_variable>
#include <ranges>
#include <any>
#include <gflags/gflags.h>
#include "cvm/topology.hpp"
#include "cvm/type_traits.hpp"
#include "cvm/logger.hpp"
#include "cvm/random.hpp"
#include <type_traits>


DECLARE_bool(signal_async);

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
                              if (coro_)
                                coro_.destroy();
                              coro_ = other.coro_;
                              other.coro_ = nullptr;
                          }

                          return *this;
                      }

                      task(const task&) = delete;
                      task& operator=(const task&) = delete;

                      ~task() { if (coro_) { coro_.destroy(); coro_ = nullptr; } }

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
                              if (coro_)
                                coro_.destroy();
                              coro_ = other.coro_;
                              other.coro_ = nullptr;
                          }

                          return *this;
                      }

                      task(const task&) = delete;
                      task& operator=(const task&) = delete;

                      ~task() { if (coro_) {coro_.destroy(); coro_ = nullptr;} }

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

                          // can have multiple channels
                          std::for_each(channels_[loc].begin(), channels_[loc].end(),
                              [&t, &handles] (auto& channel) {
                                  auto ready = channel.handles.end();
                                  for (auto it = channel.handles.begin(); it != channel.handles.end(); ++it) {
                                      auto& handle = *it;
                                      if (handle.first && (!(handle.second) || ((handle.second)(t)))) {
                                          assert((ready == channel.handles.end()) && "multiple filters passing for a channel");
                                          ready = it;
                                          handles.emplace_back(std::move(handle.first));

                                          // resuming immediately, push to the front
                                          channel.vals.emplace_front(t);
                                      }
                                  }

                                  if (ready != channel.handles.end()) {
                                      // fast vector erase of handle
                                      auto back = channel.handles.end() - 1;
                                      if (ready != back)
                                          *ready = std::move(*back);
                                      channel.handles.pop_back();
                                  }
                                  else // later will swap
                                      channel.vals.emplace_back(t);
                          });


                          // resume awaiting tasks and listeners
                          // We don't distinguish between lifetimes of "forked" tasks and normal invoked tasks. In the case of
                          // normal invoked tasks, their coroutine state could have been destroyed after resuming (task object
                          // was destructed). Therefore, we can't query whether it's done. This is largely inconsequential other than
                          // for messenger-managed lifetimes for fork tasks (tasks_). So, how do we know when we should clean up those tasks?
                          // One option would be to have it remove itself once it's determined to have finished by adding a wrapper around
                          // the user coroutine (potentially slow). The other would be to just randomly sample that a task is done every time a run occurs.
                          // Then, we initiate a cleanup on all the tasks for the ones that are finished.
                          bool clean = handles.size() > 0;
                          std::for_each(handles.begin(), handles.end(),
                              [] (const auto& handle) {
                                  handle.resume();
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

                              bool await_ready() noexcept { return false; };
                              void await_suspend(std::coroutine_handle<> awaiting) noexcept { self.moments_[loc].emplace_back(&t, awaiting); };
                              T await_resume() noexcept { return t; };
                          };

                          co_return co_await awaiter{*this, loc, T{}};
                      }

                      // messenger managed channel
                      struct channel_info{ size_t id; cvm::topology::loc_t loc; };
                      auto create_channel(cvm::topology::loc_t loc) {
                          channels_[loc].emplace_back();
                          return channel_info{channels_[loc].size() - 1, loc};
                      }

                      task<T> wait(channel_info info, const std::function<bool(const T&)>& filter) {
                          struct awaiter {
                              pool<T>& self;
                              channel_info info;
                              std::function<bool(const T&)> filter;

                              bool await_ready() noexcept {
                                  auto& channel = self.channels_[info.loc][info.id].vals;
                                  if (filter) {
                                      for (auto it = channel.begin(); it != channel.end(); ++it)
                                          if ((filter)(*it)) {
                                              if (it != channel.begin())
                                                  std::iter_swap(channel.begin(), it);
                                              return true;
                                          }
                                      return false;
                                  }
                                  else
                                      return !channel.empty();
                              };
                              void await_suspend(std::coroutine_handle<> awaiting) noexcept {
                                  self.channels_[info.loc][info.id].handles.emplace_back(awaiting, std::move(filter));
                              };
                              T await_resume() noexcept {
                                  auto& channel = self.channels_[info.loc][info.id].vals;
                                  auto val = std::move(channel.front());
                                  channel.pop_front();
                                  return val;
                              };
                          };

                          if (info.id >= channels_[info.loc].size())
                              assert(false && "channel id is invalid");
                          else
                              co_return co_await awaiter{*this, info, filter};
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
                          channel() = default;

                          std::deque<T> vals;
                          std::vector<std::pair<std::coroutine_handle<>, std::function<bool(const T&)>>> handles;
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
                  // Potential fix is to have forked remove itself from
                  // tasks upon completion.
                  forked.resume();
                  if (!forked.done()) {
                      std::lock_guard<std::mutex> guard(tasks_mutex_);
                      tasks_.emplace_back(std::move(forked));
                  }
                  return;
              }

              enum priority {
                  lowest_priority = 0,
                  highest_priority = 1,
                  num_priority = 2
              };
              static constexpr priority default_priority = lowest_priority;

            private:

              enum launch {
                  async     = 0,
                  immediate = 1,
              };

              template <typename T, typename E, typename A = const T&&>
              void _signal(cvm::topology::loc_t loc, const A m, priority prio = default_priority, launch l = immediate) {

                  if (loc == cvm::topology::null) {
                      assert(false && "attempting to signal to null location");
                      return;
                  }

                  if (prio > highest_priority) {
                      assert(false && "bad priority");
                      return;
                  }

                  cvm::log(cvm::DEBUG, "[messenger] signal to location {} of type {}\n", loc, cvm::type_traits::name<decltype(m)>());

                  static const auto key = std::type_index(typeid(E));
                  typedef std::vector<std::pair<cvm::topology::loc_t, E>> storage_t;

                  static constexpr auto f = [](std::size_t idx, messenger& m, decltype(signal_storage_[0])& s) {
                      storage_t& storage = std::any_cast<storage_t&>(s[key]);
                      auto& [loc, a] = storage[idx];
                      bool clean = m.message_pool<T>()->run(std::move(loc), std::move(a));
                      if (idx == storage.size()-1) {
                          storage.clear();
                      }
                      return clean;
                  };

                  if (l == async) {
                      {
                          std::lock_guard<std::mutex> sl(signal_mutex_);
                          auto sit = signal_storage_[prio].find(key);
                          if (sit == signal_storage_[prio].end()) {
                              sit = signal_storage_[prio].emplace(key, std::make_any<storage_t>()).first;
                          }
                          storage_t& storage = std::any_cast<storage_t&>(sit->second);

                          signal_queue_[prio].emplace_back(std::move(storage.size()), std::move(f));
                          storage.emplace_back(std::move(loc), std::move(m));
                      }
                      signal_queue_updated_.test_and_set();
                      signal_queue_updated_.notify_one();

                      if (!FLAGS_signal_async) flush();
                  } else {
                      bool clean = false;

                      if constexpr (std::is_same_v<E, std::remove_cvref_t<A>>) {
                          clean = message_pool<T>()->run(std::move(loc), std::move(m));
                      } else {
                          clean = message_pool<T>()->run(std::move(loc), E(std::move(m)));
                      }

                      if (clean) {
                          clean_tasks();
                      }
                  }

                  return;
              }

            public:

              template <typename T>
              void signal(cvm::topology::loc_t loc, const T& m) {
                  _signal<T, T, const T&>(loc, m, default_priority, immediate);
              }

              template <typename T>
              void signal_async(cvm::topology::loc_t loc, const T& m, priority prio = default_priority) {
                  _signal<T, T, const T&>(loc, m, prio, async);
              }

              template <typename T, typename E, typename A = const T&&>
              void signal_async(cvm::topology::loc_t loc, const A m, priority prio = default_priority) {
                  _signal<T, E, A>(loc, m, prio, async);
              }

              template <typename T>
              task<T> wait(cvm::topology::loc_t loc) {
                  co_return co_await message_pool<T>()->wait(loc);
              }

              template <typename T>
              task<T> wait(typename pool<T>::channel_info info, const std::function<bool(const T&)>& filter = nullptr) {
                  co_return co_await message_pool<T>()->wait(info, filter);
              }

              template <typename T>
              auto channel(cvm::topology::loc_t loc) {
                  return message_pool<T>()->create_channel(loc);
              }

              template <typename T>
              void del(typename pool<T>::channel_info info) {
                  message_pool<T>()->delete_channel(info);
                  return;
              }

              void build();
              void clear();
              void flush();

          private:

              template <typename T>
              std::shared_ptr<pool<T>> message_pool() {
                  auto key = std::type_index(typeid(T));
                  std::lock_guard<std::mutex> guard(pools_mutex_);
                  auto it = pools_.find(key);
                  if (it == pools_.end()) {
                      std::shared_ptr<pool_base> p = std::make_shared<pool<T>>();
                      it = pools_.emplace(key, std::move(p)).first;
                  }
                  return std::dynamic_pointer_cast<pool<T>>(it->second);
              }

              void clean_tasks() {
                  std::lock_guard<std::mutex> guard(tasks_mutex_);
                  if (tasks_.size() > 0)
                    {
                      // To prevent HOL blocking, we sample a random index.
                      unsigned ix = cvm::rand::lcg::generate(tasks_.size());
                      if (tasks_.at(ix).done())
                        tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(),
                                    [] (const auto& handler) { return handler.done(); }), tasks_.end());
                    }
              }

              std::vector<task<void>> tasks_;
              std::mutex tasks_mutex_;
              std::unordered_map<std::type_index, std::shared_ptr<pool_base>> pools_;
              std::mutex pools_mutex_;

              std::mutex signal_mutex_;
              std::thread signal_thread_;
              std::array<std::unordered_map<std::type_index, std::any>, num_priority> signal_storage_;
              std::array<std::vector<std::pair<std::size_t, std::function<bool(std::size_t, messenger&, decltype(signal_storage_[0])&)>>>, num_priority> signal_queue_;
              std::atomic_flag quit_ = ATOMIC_FLAG_INIT;
              std::atomic_flag signal_queue_updated_ = ATOMIC_FLAG_INIT;
      };
}
