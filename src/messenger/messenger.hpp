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
#include <chrono>
#include <gflags/gflags.h>
#include <span>
#include <optional>
#include "cvm/topology.hpp"
#include "cvm/type_traits.hpp"
#include "cvm/logger.hpp"
#include "cvm/random.hpp"
#include <type_traits>


DECLARE_bool(signal_async);

#define CVM_MESSENGER_procedure_call(name, func_type) \
struct name : cvm::messenger::procedure_call_function<func_type> {};


namespace cvm {

      class messenger {

          public:

              template <typename T>
              struct procedure_call_function {
                using function_type = T;
              };

              template <typename T>
              struct function_traits;

              template <typename R, typename... Args>
              struct function_traits <R (Args...)> {
                using return_type = R;
              };

              template <typename T>
              using return_type_t = typename function_traits<typename T::function_type>::return_type;

              class procedure_call_base {
                  public:
                      virtual ~procedure_call_base() {};
              };

              template <typename F>
              class procedure_call : public procedure_call_base {
                  // represents a group of functions that listen to a type F from various locations

                  public:
                      virtual ~procedure_call() {};

                      typedef std::function<typename F::function_type> listener_t;

                      void add_listener(cvm::topology::loc_t loc, std::function<typename F::function_type> f) {
                          assert((!listeners_.contains(loc)) && "listener already exists");

                          listeners_[loc] = f;
                          cvm::log(cvm::DEBUG, "[messenger] procedure call added listener to loc {}, type {} has {} listeners\n", loc, cvm::type_traits::name<decltype(f)>(), listeners_.size());
                      }

                      template <typename... Args>
                      return_type_t<F> run(cvm::topology::loc_t loc, Args... args) {
                          auto f = listeners_[loc];
                          cvm::log(cvm::DEBUG, "[messenger] procedure call running listener at loc {}, type {}\n", loc, cvm::type_traits::name<decltype(f)>());
                          return f(args...);
                      }

                  private:
                      std::unordered_map<cvm::topology::loc_t, listener_t> listeners_;
              };

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

                          std::for_each(channels_[loc].begin(), channels_[loc].end(),
                              [&t, &handles] (auto& channel) {
                                  bool found_waiter = false;
                                  for (auto it = channel.waiters.begin(); it != channel.waiters.end();) {
                                      bool delete_waiter = false;
                                      auto& waiter = *it;
                                      if (!waiter.handle)
                                          continue;

                                      if (!waiter.wait_all && !waiter.wait_any) {
                                          const auto& filter = waiter.filters.at(0);
                                          if (!filter || filter(t)) {
                                              waiter.data[0] = t;
                                              found_waiter = true;
                                              delete_waiter = true;
                                          }
                                      }
                                      else if (waiter.wait_all) {
                                          bool check_done = false;
                                          for (unsigned i = 0; i < waiter.filters.size(); ++i) {
                                              if (waiter.data[i])
                                                  continue;

                                              const auto& filter = waiter.filters.at(i);
                                              if (!filter || filter(t)) {
                                                  waiter.data[i] = t;
                                                  found_waiter = true;
                                                  check_done = true;
                                              }
                                          }

                                          // reduce
                                          if (check_done) {
                                              bool done_waiting = true;
                                              for (unsigned i = 0; i < waiter.filters.size(); ++i)
                                                  done_waiting &= bool(waiter.data[i]);
                                              delete_waiter = done_waiting;
                                          }
                                      }
                                      else if (waiter.wait_any) {
                                          for (unsigned i = 0; i < waiter.filters.size(); ++i) {
                                              assert(!waiter.data[i]);
                                              const auto& filter = waiter.filters.at(i);
                                              if (!filter || filter(t)) {
                                                  waiter.data[i] = t;
                                                  delete_waiter = true;
                                                  found_waiter = true;
                                              }
                                          }
                                      }

                                      if (delete_waiter) {
                                          handles.emplace_back(std::move(waiter.handle));
                                          it = channel.waiters.erase(it);
                                      }
                                      else
                                        ++it;
                                  }

                                  if (!found_waiter)
                                    channel.orphans.emplace_back(t);
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
                          std::optional<T> data;

                          struct awaiter {
                              pool<T>& self;
                              std::optional<T>& data;
                              channel_info info;
                              std::function<bool(const T&)> filter;

                              bool await_ready() noexcept {
                                  auto& orphans = self.channels_[info.loc][info.id].orphans;
                                  if (orphans.empty())
                                    return false;

                                  if (filter) {
                                      for (auto it = orphans.begin(); it != orphans.end(); ++it) {
                                          if ((filter)(*it)) {
                                              data = std::move(*it);
                                              orphans.erase(it);
                                              return true;
                                          }
                                      }
                                  }
                                  else {
                                      data = orphans.front();
                                      orphans.pop_front();
                                      return true;
                                  }
                                  return false;
                              };
                              void await_suspend(std::coroutine_handle<> awaiting) noexcept {
                                  self.channels_[info.loc][info.id].waiters.emplace_back(awaiting, filter, &data);
                              };
                              T await_resume() noexcept {
                                  return data.value();
                              };
                          };

                          if (info.id >= channels_[info.loc].size())
                              assert(false && "channel id is invalid");
                          else
                              co_return co_await awaiter{*this, data, info, filter};
                      }

                      template <typename... Fs>
                        requires (std::convertible_to<Fs, std::function<bool(const T&)>> && ...)
                      task<cvm::type_traits::make_repeat_tuple_t<T, sizeof...(Fs)>> wait_all(channel_info info, Fs... filters) {
                          using array_t = std::array<std::optional<T>, sizeof...(Fs)>;
                          array_t data;

                          struct awaiter {
                              pool<T>& self;
                              array_t& data;
                              channel_info info;
                              std::vector<std::function<bool(const T&)>> filters;

                              bool await_ready() noexcept {
                                  auto& orphans = self.channels_[info.loc][info.id].orphans;
                                  if (orphans.empty())
                                    return false;

                                  int found = 0;
                                  for (auto it = orphans.begin(); it != orphans.end() && found != data.size();) {
                                      bool removable = false;
                                      for (unsigned i = 0; i < filters.size(); ++i) {
                                          if (data.at(i))
                                            continue;

                                          const auto& filter = filters.at(i);
                                          if (filter) {
                                              if ((filter)(*it)) {
                                                data.at(i) = *it;
                                                ++found;
                                                removable = true;
                                              }
                                          }
                                          else {
                                            data.at(i) = *it;
                                            ++found;
                                            removable = true;
                                          }
                                      }

                                      if (removable)
                                        it = orphans.erase(it);
                                      else
                                        ++it;
                                  }
                                  return found == data.size();
                              };
                              void await_suspend(std::coroutine_handle<> awaiting) noexcept {
                                  self.channels_[info.loc][info.id].waiters.emplace_back(awaiting, filters, data, true, false);
                              };
                              cvm::type_traits::make_repeat_tuple_t<T, sizeof...(Fs)> await_resume() noexcept {
                                  return cvm::type_traits::array_opt_to_tuple(data);
                              };
                          };

                          if (info.id >= channels_[info.loc].size())
                              assert(false && "channel id is invalid");
                          else
                              co_return co_await awaiter{*this, data, info, {std::forward<Fs>(filters)...}};
                      }

                      template <typename... Fs>
                        requires (std::convertible_to<Fs, std::function<bool(const T&)>> && ...)
                      task<cvm::type_traits::make_repeat_tuple_t<std::optional<T>, sizeof...(Fs)>> wait_any(channel_info info, Fs... filters) {
                          using array_t = std::array<std::optional<T>, sizeof...(Fs)>;
                          array_t data;

                          struct awaiter {
                              pool<T>& self;
                              array_t& data;
                              channel_info info;
                              std::vector<std::function<bool(const T&)>> filters;

                              bool await_ready() noexcept {
                                  auto& orphans = self.channels_[info.loc][info.id].orphans;
                                  if (orphans.empty())
                                    return false;

                                  int found = 0;
                                  for (auto it = orphans.begin(); it != orphans.end() && found != data.size();) {
                                      bool removable = false;
                                      for (unsigned i = 0; i < filters.size(); ++i) {
                                          if (data.at(i))
                                            continue;

                                          const auto& filter = filters.at(i);
                                          if (filter) {
                                              if ((filter)(*it)) {
                                                data.at(i) = *it;
                                                ++found;
                                                removable = true;
                                              }
                                          }
                                          else {
                                            data.at(i) = *it;
                                            ++found;
                                            removable = true;
                                          }
                                      }

                                      if (removable)
                                        it = orphans.erase(it);
                                      else
                                        ++it;
                                  }
                                  return found > 0;
                              };
                              void await_suspend(std::coroutine_handle<> awaiting) noexcept {
                                  self.channels_[info.loc][info.id].waiters.emplace_back(awaiting, filters, data, false, true);
                              };
                              cvm::type_traits::make_repeat_tuple_t<std::optional<T>, sizeof...(Fs)> await_resume() noexcept {
                                  return std::tuple_cat(data);
                              };
                          };

                          if (info.id >= channels_[info.loc].size())
                              assert(false && "channel id is invalid");
                          else
                              co_return co_await awaiter{*this, data, info, {std::forward<Fs>(filters)...}};
                      }

                      auto get_orphans(channel_info info) {
                          return channels_[info.loc][info.id].orphans;
                      }

                      void clear_channel(channel_info info) {
                          channels_[info.loc][info.id].orphans.clear();
                          return;
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

                          struct channel_waiter {
                            channel_waiter(std::coroutine_handle<> h, const std::function<bool(const T&)>& f, std::optional<T>* t)
                              : handle(h), filters({f}), data(t, 1) {};

                            template <size_t N>
                            channel_waiter(std::coroutine_handle<> h, const std::vector<std::function<bool(const T&)>>& f,
                                           std::array<std::optional<T>, N>& ts, bool wait_all, bool wait_any)
                              : handle(h), filters(f), data(ts), wait_all(wait_all), wait_any(wait_any) {};

                            std::coroutine_handle<> handle;
                            std::vector<std::function<bool(const T&)>> filters;
                            std::span<std::optional<T>> data;
                            bool wait_all;
                            bool wait_any;
                          };

                          std::deque<T> orphans; // We insert here if there's no waiters matching on new message.
                          std::vector<channel_waiter> waiters; // Per-coroutine handle.
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
                      cvm::log(cvm::ERROR, "Error: messenger: attempting to signal to null location with type {}\n", cvm::type_traits::name<decltype(m)>());
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

                          signal_queue_[prio].emplace_back(std::move(storage.size()), std::chrono::steady_clock::now(), std::move(f));
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

              // If any filter is passing, will return T.
              template <typename T, typename... Fs>
                requires (std::convertible_to<Fs, std::function<bool(const T&)>> && ...)
              task<cvm::type_traits::make_repeat_tuple_t<std::optional<T>, sizeof...(Fs)>> wait_any(typename pool<T>::channel_info info, Fs... filters) {
                  co_return co_await message_pool<T>()->wait_any(info, std::forward<Fs>(filters)...);
              }

              // Returns matching transaction for each filter. If a transaction would apply
              // to more than one filter we keep both. Does not return until all filters
              // are passing.
              template <typename T, typename... Fs>
                requires (std::convertible_to<Fs, std::function<bool(const T&)>> && ...)
              task<cvm::type_traits::make_repeat_tuple_t<T, sizeof...(Fs)>> wait_all(typename pool<T>::channel_info info, Fs... filters) {
                  co_return co_await message_pool<T>()->wait_all(info, std::forward<Fs>(filters)...);
              }

              template <typename T>
              auto channel(cvm::topology::loc_t loc) {
                  return message_pool<T>()->create_channel(loc);
              }

              template <typename T>
              auto get_channel_orphans(typename pool<T>::channel_info info) {
                  return message_pool<T>()->get_orphans(info);
              }

              template <typename T>
              void del_channel(typename pool<T>::channel_info info) {
                  message_pool<T>()->delete_channel(info);
                  return;
              }

              template <typename T>
              void clear_channel(typename pool<T>::channel_info info) {
                  message_pool<T>()->clear_channel(info);
                  return;
              }

              template<typename F>
              void procedure(cvm::topology::loc_t loc, std::function<typename F::function_type> listener) {
                  // Connect a listener to the appropriate remote_call
                  cvm::log(cvm::DEBUG, "[messenger] procedure location {} with type {}\n", loc, typeid(F).name());
                  assert((loc != cvm::topology::null) && "attempting to register procedure to a null location");
                  procedure_calls<F>()->add_listener(loc, listener);
              }

              template<typename F, typename... Args>
              [[nodiscard]] return_type_t<F> call(cvm::topology::loc_t loc, Args&&... args) {
                  // Find a remote call and call it's function for a specified location
                  cvm::log(cvm::DEBUG, "[messenger] call location {} with type {}\n", loc, typeid(F).name());
                  assert((loc != cvm::topology::null) && "attempting to call procedure to a null location");
                  return procedure_calls<F>()->template run<Args...>(loc, args...);
              }

              void build();
              void clear();
              void flush();
              auto task_guard() { return std::lock_guard<std::recursive_mutex>(running_task_mutex_); }

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

              template <typename F>
              std::shared_ptr<procedure_call<F>> procedure_calls() {
                  auto key = std::type_index(typeid(F));
                  std::lock_guard<std::mutex> guard(procedure_calls_mutex_);
                  auto it = procedure_calls_.find(key);
                  if (it == procedure_calls_.end()) {
                      std::shared_ptr<procedure_call<F>> rc = std::make_shared<procedure_call<F>>();
                      it = procedure_calls_.emplace(key, std::move(rc)).first;
                  }
                  return std::dynamic_pointer_cast<procedure_call<F>>(it->second);
              }

              std::vector<task<void>> tasks_;
              std::mutex tasks_mutex_;
              std::unordered_map<std::type_index, std::shared_ptr<pool_base>> pools_;
              std::mutex pools_mutex_;

              std::mutex signal_mutex_;
              std::thread signal_thread_;
              std::array<std::unordered_map<std::type_index, std::any>, num_priority> signal_storage_;
              std::array<std::vector<std::tuple<std::size_t, decltype(std::chrono::steady_clock::now()), std::function<bool(std::size_t, messenger&, decltype(signal_storage_[0])&)>>>, num_priority> signal_queue_;
              std::atomic_flag quit_ = ATOMIC_FLAG_INIT;
              std::atomic_flag signal_queue_updated_ = ATOMIC_FLAG_INIT;

              std::unordered_map<std::type_index, std::shared_ptr<procedure_call_base>> procedure_calls_;
              std::mutex procedure_calls_mutex_;

              std::recursive_mutex running_task_mutex_;
      };
}
