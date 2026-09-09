// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <gflags/gflags.h>

#include "cvm/logger.hpp"
#include "cvm/pipe.hpp"
#include "cvm/pipe_internal.hpp"
#include "cvm/registry.hpp"
#include "svdpi.h"

// Owned by cvm::callbacks: tells us whether a worker already drains the queue.
DECLARE_bool(cb_async);

// Bare value or `name=value` pairs, global or per instance.
DEFINE_string(cvm_pipe_epoch, "1024",
              "elements pushed per epoch: a bare value, or name=value pairs");
DEFINE_string(cvm_pipe_headroom, "256",
              "elements still held when the next epoch is requested, sized to "
              "cover the platform's push landing latency");
DEFINE_string(cvm_pipe_suppress_push, "",
              "fault injection: never deliver pushes, forcing every element "
              "through the relief path");

extern "C" void cvm_pipe_in_push(unsigned int count,
                                 const unsigned int* data_words,
                                 unsigned char epoch, unsigned char is_last);
extern "C" void cvm_pipe_in_reset();

namespace {

  std::vector<std::string>& handles() {
    static std::vector<std::string> h;
    return h;
  }

  int int_from_keyed(const std::string& flag, const std::string& name,
                     int fallback) {
    const auto v = cvm::pipe::resolve_keyed(flag, name);
    if (!v.has_value())
      return fallback;
    try {
      return std::stoi(*v);
    } catch (...) {
      cvm::log(cvm::ERROR, "cvm::pipe: {}: cannot parse `{}` as a number\n", name,
               *v);
      return fallback;
    }
  }

  // The repo's one queue: cvm::registry::build() owns its lifecycle and every
  // transactor pushes here, so a private instance would mean a second async
  // worker and a queue nothing else drains.
  //
  // Spelled out, not aliased: inside namespace cvm::pipe an unqualified
  // `callbacks` resolves to the *class* and silently builds an empty temporary.
  cvm::callbacks& cb_queue() { return cvm::registry::callbacks; }

  // Hands queued exports to the HDL, at the pipe's own trigger points, so a
  // testbench that never calls into C++ still gets its data.
  //
  // Asynchronous mode needs a platform that serialises exports against clock
  // edges; without that the worker writes the queue while the design reads it,
  // which under Verilator reliably reorders and splits elements. A worker also
  // owns flush() and blocks in it, so calling it here would never return.
  void deliver() {
    if (!FLAGS_cb_async)
      cb_queue().flush();
  }

  cvm::pipe::stream_state* stream_for(int handle) {
    if (handle < 0 || static_cast<std::size_t>(handle) >= handles().size()) {
      cvm::log(cvm::ERROR, "cvm::pipe: invalid handle {}\n", handle);
      return nullptr;
    }
    return cvm::pipe::find_stream(handles()[static_cast<std::size_t>(handle)]);
  }

  // Returns an unsent batch to the head of the queue; caller holds s->mutex.
  // Put back rather than forwarded, so there is one source of truth for what has
  // been delivered.
  void reclaim(cvm::pipe::stream_state& s) {
    const std::shared_ptr<cvm::pipe::batch> b = s.outstanding;
    if (!b || b->claimed)
      return;
    b->claimed = true;
    s.outstanding.reset();

    const std::size_t n = b->count * s.words_per_element;
    s.words.insert(s.words.begin(), b->words.begin(),
                   b->words.begin() + static_cast<std::ptrdiff_t>(n));
    // The end-of-stream marker went with it, so let a later epoch carry it.
    if (b->is_last)
      s.sent_last = false;
  }

} // namespace

extern "C" {

// `epoch_max_elements` is what the consumer's formal holds, so the host clamps
// to that rather than a constant of its own. build() is deliberately not called
// here: the registry owns that, and building twice spawns a second worker.
int cvm_pipe_open(const char* name, int epoch_max_elements) {
  cvm::pipe::stream_state& s = cvm::pipe::ensure_stream(name);
  s.scope = svGetScope();
  s.bound = true;
  s.epoch_cap = epoch_max_elements > 0
                    ? static_cast<std::size_t>(epoch_max_elements)
                    : 1;
  s.suppress_push =
      cvm::pipe::resolve_keyed(FLAGS_cvm_pipe_suppress_push, name).has_value();
  handles().push_back(name);

  // Zero the C-owned write pointer through the export; RTL must not write it.
  // The queue is FIFO, so it lands before any push for this stream.
  cb_queue().push(static_cast<svScope>(s.scope),
                  std::function<void()>([] { cvm_pipe_in_reset(); }));
  deliver();

  return static_cast<int>(handles().size()) - 1;
}

int cvm_pipe_epoch_size(const char* name) {
  return int_from_keyed(FLAGS_cvm_pipe_epoch, name, 1024);
}

int cvm_pipe_headroom(const char* name) {
  return int_from_keyed(FLAGS_cvm_pipe_headroom, name, 256);
}

// Confirms `epoch` consumed and sends the next. Void, so the clock keeps
// running -- which is also why pulling from a producer is safe here: encoding
// costs wall-clock but no stalled cycles.
void cvm_pipe_request(int handle, int epoch, int room) {
  cvm::pipe::stream_state* s = stream_for(handle);
  if (s == nullptr)
    return;

  const std::string& name = handles()[static_cast<std::size_t>(handle)];
  std::size_t wanted = static_cast<std::size_t>(
      int_from_keyed(FLAGS_cvm_pipe_epoch, name, 1024));
  if (wanted > s->epoch_cap) {
    // Never silent: the epoch amortises the host round trip, so a clamp is a
    // throughput cliff that would otherwise look like unexplained slowness.
    if (!s->clamp_reported) {
      s->clamp_reported = true;
      cvm::log(cvm::ERROR,
               "cvm::pipe: {}: epoch of {} elements clamped to {}, which the "
               "consumer's push can hold; raise EPOCH_MAX_ELEMENTS or lower "
               "+cvm_pipe_epoch\n",
               name, wanted, s->epoch_cap);
    }
    wanted = s->epoch_cap;
  }
  // Top up first, so a streaming producer is not driven from the relief path.
  cvm::pipe::in_stream(name, s->words_per_element).ensure(wanted);

  auto b = std::make_shared<cvm::pipe::batch>();
  {
    // Serialised against producers so a push cannot interleave with assembly.
    std::lock_guard<std::mutex> lock(s->mutex);
    if (s->sent_last)
      return;
    // A request supersedes an unsent batch rather than stacking behind it.
    reclaim(*s);

    const std::size_t wpe = s->words_per_element;
    if (wpe == 0)
      return;
    int limit = static_cast<int>(wanted);
    if (room >= 0 && room < limit)
      limit = room;
    if (limit <= 0)
      return;

    // Full sized: a simulator may read the whole formal.
    b->words.assign(cvm::pipe::kMaxWords, 0u);
    while (b->count < static_cast<unsigned int>(limit) &&
           s->words.size() >= wpe) {
      for (std::size_t w = 0; w < wpe; ++w) {
        b->words[b->count * wpe + w] = s->words.front();
        s->words.pop_front();
      }
      ++b->count;
    }
    b->is_last = s->closed && s->words.empty();
    s->epoch = static_cast<std::uint32_t>(epoch) + 1;
    b->epoch = static_cast<unsigned char>(s->epoch & 0xFFu);
    if (b->is_last)
      s->sent_last = true;

    if (b->count == 0 && !b->is_last)
      return;
    s->outstanding = b;
  }

  // Queued, not called, so the export runs after this import returns and no
  // lock is held across it. Until then relief can still reclaim the batch.
  if (!s->suppress_push) {
    cvm::pipe::stream_state* sp = s;
    cb_queue().push(static_cast<svScope>(s->scope),
                    std::function<void()>([sp, b] {
                      std::lock_guard<std::mutex> lock(sp->mutex);
                      if (b->claimed)
                        return;
                      b->claimed = true;
                      if (sp->outstanding == b)
                        sp->outstanding.reset();
                      cvm_pipe_in_push(b->count, b->words.data(), b->epoch,
                                       b->is_last ? 1 : 0);
                    }));
  }

  deliver();
}

// Fallback: return elements inline because a push has not landed in time. The
// array is open so each consumer can size its own relief buffer.
int cvm_pipe_relief(int handle, int max_elements,
                    const svOpenArrayHandle words, unsigned int* is_last) {
  if (is_last != nullptr)
    *is_last = 0;
  cvm::pipe::stream_state* s = stream_for(handle);
  if (s == nullptr || max_elements <= 0)
    return 0;

  auto* data_words = static_cast<unsigned int*>(svGetArrayPtr(words));
  if (data_words == nullptr) {
    cvm::log(cvm::ERROR,
             "cvm::pipe: relief array is not C-layout compatible\n");
    return 0;
  }
  const std::size_t room_words =
      static_cast<std::size_t>(svSizeOfArray(words)) / sizeof(unsigned int);

  // The starved case: the clock is already stopped, so pull synchronously.
  cvm::pipe::in_stream(handles()[static_cast<std::size_t>(handle)],
                       s->words_per_element)
      .ensure(static_cast<std::size_t>(max_elements));

  std::lock_guard<std::mutex> lock(s->mutex);
  // Take back whatever is still in flight, so these elements are delivered
  // exactly once and in order.
  reclaim(*s);

  const std::size_t wpe = s->words_per_element;
  if (wpe == 0)
    return 0;
  std::size_t limit = static_cast<std::size_t>(max_elements);
  if (room_words / wpe < limit)
    limit = room_words / wpe;

  unsigned int count = 0;
  while (count < limit && s->words.size() >= wpe) {
    for (std::size_t w = 0; w < wpe; ++w) {
      data_words[count * wpe + w] = s->words.front();
      s->words.pop_front();
    }
    ++count;
  }

  // End of stream travels this path too: the push carrying it was reclaimed.
  if (s->closed && s->words.empty()) {
    if (is_last != nullptr)
      *is_last = 1;
    s->sent_last = true;
  }
  return static_cast<int>(count);
}

} // extern "C"
