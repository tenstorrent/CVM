// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "cvm/replay_encoder.hpp"
#include "cvm/replay_source.hpp"
#include "cvm/pipe.hpp"
#include "cvm/topology.hpp"

namespace cvm {
  namespace replay {

    // One port of the boundary, declared before the load.
    struct bind_request {
        const char* name = nullptr;
        int width = 0;
        int bit_offset = 0;
        bool is_output = false;
        int* status = nullptr;
        std::atomic<bool>* done = nullptr;
    };

    // A dump port that is deliberately not replayed: the clock, or one the
    // spec excluded. Declared the same way and at the same time as a bind.
    struct ignore_request {
        const char* name = nullptr;
        int* status = nullptr;
        std::atomic<bool>* done = nullptr;
    };

    struct load_request {
        int port_bits = 0;
        // The transport's element size, computed by the generated module so
        // this side does not re-derive it.
        int elem_words = 0;
        int* status = nullptr;
        std::atomic<bool>* done = nullptr;
    };

    // One word of the failure mask, staged until the summary arrives.
    struct report_word {
        int index = 0;
        std::uint32_t bits = 0;
    };

    struct report {
        std::int64_t mismatches = 0;
        std::int64_t first_fail_cycle = -1;
        std::int64_t cycles = 0;
        int min_occupancy = 0;
        int demands = 0;
        std::atomic<bool>* done = nullptr;
    };

    // One replayed interposer. Registered against the location the testbench
    // passes to the module, and owns the transport at that same location, so a
    // consumer declares one node and gets both.
    class engine {
      public:
        engine(cvm::topology::loc_t loc, unsigned id);

        // Called once at end of run, by cvm::registry::check().
        void check() const;

      private:
        int bind(const bind_request& r);
        int ignore(const ignore_request& r);
        int load(const load_request& r);
        void install_producer(std::size_t words_per_element);
        std::size_t produce(std::uint32_t* out, std::size_t max_elements);

        cvm::topology::loc_t loc_;
        std::string who_;
        cvm::pipe_in pipe_;

        std::ifstream file_;
        source src_;
        encoder encoder_;

        std::vector<std::uint32_t> fail_bits_;
        // Staged by bind(), applied in load(): the binds arrive before the
        // recording is open, and the SV string they name does not outlive the
        // call.
        struct binding {
            std::string name;
            std::size_t width = 0;
            std::size_t bit_offset = 0;
            bool is_output = false;
        };
        std::vector<binding> bindings_;
        std::vector<std::string> ignored_;

        std::string path_;
        bool reported_ = false;
    };

  } // namespace replay
} // namespace cvm
