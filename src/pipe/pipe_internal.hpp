// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

// Shared between pipe.cpp and pipe_dpi.cpp. Consumers use cvm/pipe.hpp.

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "cvm/pipe.hpp"

namespace cvm {
  namespace pipe {

    // A queued push, from assembly until the export runs. Shared with the callback
    // closure so the relief path can take it back.
    struct batch {
        std::vector<std::uint32_t> words;
        unsigned int count = 0;
        unsigned char epoch = 0;
        bool is_last = false;
        // Whichever side gets here first wins: the closure sends, or relief
        // reclaims. The loser does nothing.
        bool claimed = false;
    };

    struct stream_state {
        mutable std::mutex mutex;
        // Flattened: words_per_element words per queued element.
        std::deque<std::uint32_t> words;
        std::size_t words_per_element = 1;
        bool closed = false;

        void* scope = nullptr;
        bool bound = false;

        // Only the low 8 bits reach the HDL, enough to tell one request from the
        // next.
        std::uint32_t epoch = 0;
        bool sent_last = false;

        // Handed to a callback but not yet known to have been sent.
        std::shared_ptr<batch> outstanding;

        // Fault injection: drop pushes, so relief can be exercised where they
        // otherwise always land in time.
        bool suppress_push = false;

        // Pulled from when the queue runs short. Called from ensure(), before the
        // lock, so never under `mutex`.
        in_stream::producer_fn producer;

        // Elements one push may carry, as the consumer reported at open.
        std::size_t epoch_cap = 0;
        bool clamp_reported = false;
    };

    stream_state* find_stream(const std::string& name);
    stream_state& ensure_stream(const std::string& name);

  } // namespace pipe
} // namespace cvm
