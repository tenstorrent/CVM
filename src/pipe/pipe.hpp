// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "cvm/topology.hpp"

namespace cvm {

  // Delivery may happen on another thread, so a message with a result pointer
  // also carries the flag its caller waits on.
  struct pipe_producer {
      // Returns how many elements it wrote. 0 means none available right now;
      // send pipe_close to say there will never be more.
      std::function<std::size_t(std::uint32_t* out, std::size_t max_elements)> fill;
      std::size_t words_per_element = 0;
  };

  struct pipe_close {};

  struct pipe_reset {
      std::uint32_t depth = 0;
      std::uint32_t words_per_element = 0;
      std::uint32_t slot_words = 0;
      void (*push)(unsigned int, const unsigned int*, unsigned char) = nullptr;
  };

  struct pipe_credits {
      std::uint32_t rptr = 0;
  };

  struct pipe_demand {
      std::uint32_t rptr = 0;
      // 1 pushed, 0 finished, -1 nothing right now.
      int* status = nullptr;
      std::atomic<bool>* done = nullptr;
  };

  struct pipe_pending {
      std::size_t* elements = nullptr;
      std::atomic<bool>* done = nullptr;
  };

  // The only state two threads share. It guards itself so that the producer
  // call and the DPI export cannot end up inside the lock.
  class stream_buffer {
    public:
      struct taken {
          std::size_t words = 0;
          bool is_last = false;
      };

      void reset();
      void close();
      void append(const std::uint32_t* words, std::size_t count);
      taken take(std::size_t max_words, std::uint32_t* dest);
      std::size_t size() const;

    private:
      mutable std::mutex m_;
      std::deque<std::uint32_t> words_;
      bool closed_ = false;
  };

  // Host side of cvm_pipe_in. Registered by whoever owns it -- usually the
  // producer -- not by this library.
  class pipe_in {
    public:
      pipe_in(cvm::topology::loc_t loc, unsigned id);

    private:
      // Only ever touched on the messenger's thread, which services one handler
      // at a time, so none of this needs a lock.
      void pull();
      void queue_drain();
      bool configured() const;

      cvm::topology::loc_t loc_;
      decltype(pipe_producer::fill) producer_;
      std::vector<std::uint32_t> pull_batch_;

      std::size_t wpe_ = 0;
      std::uint32_t depth_ = 0;
      std::uint32_t slot_words_ = 0;
      // Reserved at pull time, so the buffer never holds more than the design
      // has room for and draining needs no space arithmetic.
      std::uint32_t wptr_ = 0;
      std::uint32_t rptr_ = 0;
      decltype(pipe_reset::push) push_ = nullptr;

      stream_buffer buffer_;

      // 1 pushed, 0 finished, -1 ask again next cycle.
      int demand();

      // Requires dpi_mutex_.
      int drain(decltype(pipe_reset::push) push, std::size_t slot_words,
                std::size_t wpe);

      // Which thread may push, so a demand-time push and a queued one stay in
      // order and can share `batch_`.
      std::mutex dpi_mutex_;
      std::vector<std::uint32_t> batch_;
      std::atomic<bool> sent_last_ = false;
  };

  namespace pipe {
    // A bare value, or `KEY=value` pairs keyed by topology path. Bare applies
    // everywhere; a path covers every instance at it.
    std::optional<std::string> resolve_keyed(const std::string& flag_value,
                                             cvm::topology::loc_t loc);
  } // namespace pipe
} // namespace cvm
