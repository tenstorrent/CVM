// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace cvm {
  namespace pipe {

    // Matches CVM_PIPE_MAX_WORDS in cvm_pipe_pkg.sv. A simulator may read the whole
    // formal, so the host hands over a full-sized buffer.
    constexpr std::size_t kMaxWords = 8192;

    // Host side of a named pipe. Elements are queued here and pushed an epoch at a
    // time when the HDL asks, so the host never guesses at free space; a late push
    // is fetched inline instead. Either way elements come off this one queue, in
    // order, exactly once.
    //
    // Nothing services this from a simulation loop -- the pipe delivers when asked,
    // so a testbench needs no host-side driver.
    //
    // The payload is opaque: `words_per_element` words, meaning the consumer's
    // business.
    class in_stream {
      public:
        in_stream(std::string name, std::size_t words_per_element);

        in_stream(const in_stream&) = delete;
        in_stream& operator=(const in_stream&) = delete;

        // `words` must hold words_per_element entries.
        void push(const std::uint32_t* words);
        void push(const std::vector<std::uint32_t>& words);

        // The final epoch is marked as last, so end of stream travels the data path
        // rather than a side channel.
        void close();

        std::size_t pending() const;

        // Fills `out` with up to `max_elements` elements, returning how many were
        // written; 0 means end of stream and the stream closes itself. `out` holds
        // max_elements * words_per_element words.
        //
        // A buffer rather than the stream, so a producer cannot re-enter the lock or
        // leave the queue half-updated.
        using producer_fn =
            std::function<std::size_t(std::uint32_t* out, std::size_t max_elements)>;

        // Without one the stream is push-only.
        void on_demand(producer_fn fn);

        // Called by the transport, not by producers.
        void ensure(std::size_t elements);

      private:
        std::string name_;
        std::size_t words_per_element_;
    };

    // A bare value or `key=value` pairs, so one flag serves globally or per
    // instance: cvm_plusargs cannot key flag *names*, so keying is in the value.
    std::optional<std::string> resolve_keyed(const std::string& flag_value,
                                             const std::string& key);

  } // namespace pipe
} // namespace cvm
