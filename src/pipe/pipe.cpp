// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/pipe.hpp"

#include <map>
#include <mutex>

#include "cvm/pipe_internal.hpp"

namespace cvm {
  namespace pipe {

    namespace {

      std::map<std::string, stream_state>& registry() {
        // Function-local: streams may be built during static initialization.
        static std::map<std::string, stream_state> streams;
        return streams;
      }

      std::mutex& registry_mutex() {
        static std::mutex m;
        return m;
      }

    } // namespace

    stream_state* find_stream(const std::string& name) {
      std::lock_guard<std::mutex> lock(registry_mutex());
      const auto it = registry().find(name);
      return it == registry().end() ? nullptr : &it->second;
    }

    stream_state& ensure_stream(const std::string& name) {
      std::lock_guard<std::mutex> lock(registry_mutex());
      return registry()[name];
    }

    in_stream::in_stream(std::string name, std::size_t words_per_element)
        : name_(std::move(name)), words_per_element_(words_per_element) {
      stream_state& s = ensure_stream(name_);
      std::lock_guard<std::mutex> lock(s.mutex);
      s.words_per_element = words_per_element;
    }

    void in_stream::push(const std::uint32_t* words) {
      stream_state& s = ensure_stream(name_);
      std::lock_guard<std::mutex> lock(s.mutex);
      s.words.insert(s.words.end(), words, words + words_per_element_);
    }

    void in_stream::push(const std::vector<std::uint32_t>& words) {
      push(words.data());
    }

    void in_stream::close() {
      stream_state& s = ensure_stream(name_);
      std::lock_guard<std::mutex> lock(s.mutex);
      s.closed = true;
    }

    void in_stream::on_demand(producer_fn fn) {
      stream_state& s = ensure_stream(name_);
      std::lock_guard<std::mutex> lock(s.mutex);
      s.producer = std::move(fn);
    }

    void in_stream::ensure(std::size_t elements) {
      stream_state& s = ensure_stream(name_);
      // Copied out and the lock released first, so a producer touching this same
      // stream cannot deadlock.
      producer_fn fn;
      {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (!s.producer || s.closed)
          return;
        fn = s.producer;
      }

      std::vector<std::uint32_t> batch;
      while (pending() < elements) {
        const std::size_t want = elements - pending();
        batch.assign(want * words_per_element_, 0u);
        std::size_t got = fn(batch.data(), want);
        // Overstating would read past the buffer, so believe the smaller number.
        if (got > want)
          got = want;
        if (got == 0) {
          // Finished. End of stream travels the data path, so this raises eos.
          close();
          return;
        }
        std::lock_guard<std::mutex> lock(s.mutex);
        s.words.insert(s.words.end(), batch.begin(),
                       batch.begin() + static_cast<std::ptrdiff_t>(
                                           got * words_per_element_));
      }
    }

    std::size_t in_stream::pending() const {
      stream_state* s = find_stream(name_);
      if (s == nullptr)
        return 0;
      std::lock_guard<std::mutex> lock(s->mutex);
      return s->words.size() / words_per_element_;
    }

    std::optional<std::string> resolve_keyed(const std::string& flag_value,
                                             const std::string& key) {
      if (flag_value.empty())
        return std::nullopt;
      // A bare value applies to every key.
      if (flag_value.find('=') == std::string::npos)
        return flag_value;

      std::size_t pos = 0;
      while (pos <= flag_value.size()) {
        const std::size_t comma = flag_value.find(',', pos);
        const std::string entry = flag_value.substr(
            pos, comma == std::string::npos ? std::string::npos : comma - pos);
        const std::size_t eq = entry.find('=');
        if (eq != std::string::npos && entry.substr(0, eq) == key)
          return entry.substr(eq + 1);
        if (comma == std::string::npos)
          break;
        pos = comma + 1;
      }
      return std::nullopt;
    }

  } // namespace pipe
} // namespace cvm
