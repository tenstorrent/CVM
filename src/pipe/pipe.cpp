// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/pipe.hpp"

#include "cvm/logger.hpp"
#include "cvm/registry.hpp"

namespace cvm {

  namespace {

    // The waiting caller is a DPI with the clock stopped, so release it however
    // the handler ends.
    struct release {
        std::atomic<bool>* flag;
        ~release() {
          if (flag != nullptr) {
            *flag = true;
            flag->notify_one();
          }
        }
    };

  } // namespace

  void stream_buffer::reset() {
    const std::lock_guard<std::mutex> g(m_);
    words_.clear();
    closed_ = false;
  }

  void stream_buffer::close() {
    const std::lock_guard<std::mutex> g(m_);
    closed_ = true;
  }

  void stream_buffer::append(const std::uint32_t* words, std::size_t count) {
    const std::lock_guard<std::mutex> g(m_);
    words_.insert(words_.end(), words, words + count);
  }

  stream_buffer::taken stream_buffer::take(std::size_t max_words,
                                           std::uint32_t* dest) {
    const std::lock_guard<std::mutex> g(m_);
    const std::size_t n = words_.size() < max_words ? words_.size() : max_words;
    for (std::size_t i = 0; i < n; ++i) {
      dest[i] = words_.front();
      words_.pop_front();
    }
    return taken{n, closed_ && words_.empty()};
  }

  std::size_t stream_buffer::size() const {
    const std::lock_guard<std::mutex> g(m_);
    return words_.size();
  }

  pipe_in::pipe_in(cvm::topology::loc_t loc, unsigned) : loc_(loc) {
    auto& m = cvm::registry::messenger;

    m.connect<pipe_producer>(loc, [this](const pipe_producer& p) {
      // Disagreeing means the two sides have different ideas of an element.
      if (wpe_ != 0 && wpe_ != p.words_per_element) {
        cvm::log(cvm::ERROR,
                 "cvm::pipe_in({}): producer says {} words per element, the "
                 "design says {}\n",
                 loc_, p.words_per_element, wpe_);
        return;
      }
      producer_ = p.fill;
      wpe_ = p.words_per_element;
    });

    m.connect<pipe_close>(loc, [this](const pipe_close&) { buffer_.close(); });

    m.connect<pipe_reset>(loc, [this](const pipe_reset& r) {
      const std::lock_guard<std::mutex> g(dpi_mutex_);
      depth_ = r.depth;
      slot_words_ = r.slot_words;
      if (wpe_ != 0 && wpe_ != r.words_per_element) {
        cvm::log(cvm::ERROR,
                 "cvm::pipe_in({}): design says {} words per element, the "
                 "producer says {}\n",
                 loc_, r.words_per_element, wpe_);
      }
      wpe_ = r.words_per_element;
      wptr_ = 0;
      rptr_ = 0;
      push_ = r.push;
      sent_last_ = false;
      buffer_.reset();
    });

    m.connect<pipe_credits>(loc, [this](const pipe_credits& c) {
      rptr_ = c.rptr;
      pull();
      queue_drain();
    });

    m.connect<pipe_demand>(loc, [this](const pipe_demand& d) {
      const release _{d.done};
      rptr_ = d.rptr;
      pull();
      if (d.status != nullptr)
        *d.status = demand();
    });

    m.connect<pipe_pending>(loc, [this](const pipe_pending& q) {
      const release _{q.done};
      if (q.elements != nullptr)
        *q.elements = wpe_ == 0 ? 0 : buffer_.size() / wpe_;
    });
  }

  bool pipe_in::configured() const {
    return push_ != nullptr && wpe_ != 0 && depth_ != 0 &&
           slot_words_ >= wpe_;
  }

  // Takes only what the design has room for, so the buffer is a reservation and
  // the drain needs no space arithmetic.
  void pipe_in::pull() {
    if (!producer_ || !configured())
      return;
    const std::uint32_t used = wptr_ - rptr_;
    std::size_t room = depth_ > used ? depth_ - used : 0;
    while (room > 0) {
      pull_batch_.resize(room * wpe_);
      // Unlocked: this is the consumer's code, and for replay it reads a file.
      std::size_t got = producer_(pull_batch_.data(), room);
      // Overstating would read past the buffer, so believe the smaller one.
      if (got > room)
        got = room;
      if (got == 0)
        return;
      buffer_.append(pull_batch_.data(), got * wpe_);
      wptr_ += static_cast<std::uint32_t>(got);
      room -= got;
    }
  }

  void pipe_in::queue_drain() {
    if (!configured())
      return;
    // Carried by value so the draining side reads no member but the buffer.
    cvm::registry::callbacks.push(
        loc_, cvm::callbacks::cb([this, push = push_, slot = slot_words_,
                                  wpe = wpe_] {
          const std::lock_guard<std::mutex> g(dpi_mutex_);
          drain(push, slot, wpe);
        }));
  }

  int pipe_in::drain(decltype(pipe_reset::push) push, std::size_t slot_words,
                     std::size_t wpe) {
    if (sent_last_)
      return 0;
    const std::size_t cap = (slot_words / wpe) * wpe;
    if (batch_.size() != slot_words)
      batch_.assign(slot_words, 0u);
    int pushes = 0;
    for (;;) {
      const stream_buffer::taken t = buffer_.take(cap, batch_.data());
      // A closed and empty stream still owes the design one push, to carry the
      // end-of-stream flag.
      if (t.words == 0 && !t.is_last)
        break;
      push(static_cast<unsigned int>(t.words / wpe), batch_.data(),
           t.is_last ? 1 : 0);
      ++pushes;
      if (t.is_last) {
        sent_last_ = true;
        break;
      }
    }
    return pushes;
  }

  // The clock is stopped waiting on this, so it pushes rather than queues.
  int pipe_in::demand() {
    if (sent_last_)
      return 0;
    if (!configured())
      return -1;
    // Someone else is mid-push, and its data is already on the way. Jumping
    // ahead of it would reorder elements.
    std::unique_lock<std::mutex> l(dpi_mutex_, std::try_to_lock);
    if (!l.owns_lock())
      return -1;

    int pushes = 0;
    const auto push = push_;
    const auto slot = static_cast<std::size_t>(slot_words_);
    const auto wpe = wpe_;
    cvm::registry::callbacks.call(
        loc_, cvm::callbacks::cb(
                  [&] { pushes = drain(push, slot, wpe); }));
    if (pushes > 0)
      return 1;
    return sent_last_ ? 0 : -1;
  }

  namespace pipe {

    namespace {

      // Covers every instance at the path: these are rates, not per-instance
      // behaviour.
      bool key_matches(const std::string& key, cvm::topology::loc_t loc) {
        for (const cvm::topology::loc_t l :
             cvm::topology::get_from_hierarchy(key)) {
          if (l == loc)
            return true;
        }
        return false;
      }

    } // namespace

    std::optional<std::string> resolve_keyed(const std::string& flag_value,
                                             cvm::topology::loc_t loc) {
      if (flag_value.empty())
        return std::nullopt;
      if (flag_value.find('=') == std::string::npos)
        return flag_value;

      std::size_t pos = 0;
      while (pos <= flag_value.size()) {
        const std::size_t comma = flag_value.find(',', pos);
        const std::string entry = flag_value.substr(
            pos, comma == std::string::npos ? std::string::npos : comma - pos);
        const std::size_t eq = entry.find('=');
        if (eq != std::string::npos && key_matches(entry.substr(0, eq), loc))
          return entry.substr(eq + 1);
        if (comma == std::string::npos)
          break;
        pos = comma + 1;
      }
      return std::nullopt;
    }

  } // namespace pipe

} // namespace cvm
