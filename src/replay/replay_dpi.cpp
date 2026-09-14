// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <gflags/gflags.h>

#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "cvm/logger.hpp"
#include "cvm/pipe.hpp"
#include "cvm/registry.hpp"
#include "cvm/replay.hpp"
#include "svdpi.h"

DEFINE_string(cvm_replay_file, "",
              "recorded vector file: either a bare path, or key=path pairs "
              "separated by commas, keyed by each interposer's HIER parameter");
DEFINE_string(cvm_replay_mode, "REPLAY",
              "REPLAY (drive the DUT from the recording) or BYPASS (the "
              "testbench drives; the interposer is a transparent wire)");

// Replay is the producer, so replay registers the transport.
REGISTRY_register(cvm::pipe_in, PIPE, cvm::registry::all)

namespace {

  struct session {
      std::unique_ptr<std::ifstream> file;
      std::unique_ptr<cvm::replay::source> src;
      std::string hier;

      // Encoded on demand, so change detection persists between calls.
      cvm::replay::cycle_element previous;
      bool have_previous = false;
      bool exhausted = false;
      std::size_t words_per_element = 0;
      bool x_fill_one = false;
  };

  std::vector<std::unique_ptr<session>>&
  sessions() {
    static std::vector<std::unique_ptr<session>> s;
    return s;
  }

  // Mirrors CVM_REPLAY in cvm_replay_pkg.sv.
  constexpr int CVM_REPLAY_MODE = 0;

  // Staged by cvm_replay_report_word, consumed by cvm_replay_report.
  std::map<std::string, std::vector<std::uint32_t>>&
  report_bits() {
    static std::map<std::string, std::vector<std::uint32_t>> m;
    return m;
  }

  // Reports come by hier, not handle, so the engine needs no session. Without one
  // the summary is still reported, just by bit index.
  session*
  lookup_hier(const std::string& hier) {
    for (const auto& s : sessions()) {
      if (s && s->hier == hier)
        return s.get();
    }
    return nullptr;
  }

} // namespace

extern "C" {

  // 0 = REPLAY, 1 = BYPASS. Resolved once, out of reset.
  int cvm_replay_mode(const char* hier) {
    const std::string& m = FLAGS_cvm_replay_mode;
    if (m == "BYPASS" || m == "bypass")
      return 1;
    if (m == "REPLAY" || m == "replay")
      return 0;
    cvm::log(cvm::ERROR,
             "cvm::replay: {}: unknown +cvm_replay_mode `{}`; expected REPLAY or "
             "BYPASS\n",
             hier, m);
    return 1; // fail safe: do not drive a DUT from a recording we were not asked
              // for
  }

  // Installs the encoder the transport pulls from. The file is not read here:
  // elements are encoded on demand, so host memory holds only what is in
  // flight. Returns 0, or -1 on failure.
  int cvm_replay_load(unsigned int location, const char* hier,
                      const char* layout, int padded, int x_fill_one) {
    const std::size_t words = 1 + 3 * static_cast<std::size_t>(padded / 32);
    // BYPASS has nothing to replay, so close rather than let the transport ask
    // for elements that never come.
    if (cvm_replay_mode(hier) != CVM_REPLAY_MODE) {
      cvm::registry::messenger.signal<cvm::pipe_close>(location, {});
      return 0;
    }

    const auto path = cvm::replay::resolve_path(FLAGS_cvm_replay_file, hier);
    if (!path.has_value()) {
      cvm::log(cvm::ERROR,
               "cvm::replay: no vector file for `{}`; pass +cvm_replay_file=<path> "
               "or +cvm_replay_file={}=<path>\n",
               hier, hier);
      return -1;
    }

    auto s = std::make_unique<session>();
    s->file = std::make_unique<std::ifstream>(*path);
    if (!s->file->is_open()) {
      cvm::log(cvm::ERROR, "cvm::replay: cannot open `{}`\n", *path);
      return -1;
    }
    s->hier = hier;
    s->src = std::make_unique<cvm::replay::source>();
    if (!s->src->open(*s->file, layout, *path))
      return -1;

    if (static_cast<int>(s->src->words()) * 32 != padded) {
      cvm::log(cvm::ERROR,
               "cvm::replay: {}: layout needs {} bits but the interposer has "
               "{}\n",
               hier, s->src->words() * 32, padded);
      return -1;
    }
    s->words_per_element = words;
    s->x_fill_one = x_fill_one != 0;

    session* sp = s.get();
    cvm::pipe_producer producer;
    producer.words_per_element = words;
    producer.fill = [sp, location](std::uint32_t* out,
                                   std::size_t max_elements) -> std::size_t {
      if (sp->exhausted)
        return 0;

      const std::size_t nw = sp->src->words();
      std::size_t produced = 0;
      cvm::replay::cycle_element e;

      while (produced < max_elements) {
        if (!sp->src->next_cycle(e, sp->x_fill_one)) {
          sp->exhausted = true;
          cvm::registry::messenger.signal<cvm::pipe_close>(location, {});
          if (!sp->src->error().empty()) {
            cvm::log(cvm::ERROR, "cvm::replay: {}: {}\n", sp->hier,
                     sp->src->error());
          }
          break;
        }
        // Only a cycle that changes something costs an element: a recording can
        // hold a timestamp moving nothing the spec binds, the clock especially.
        if (sp->have_previous && e.same_payload(sp->previous))
          continue;

        std::uint32_t* slot = out + produced * sp->words_per_element;
        slot[0] = static_cast<std::uint32_t>(e.cycle);
        for (std::size_t i = 0; i < nw; ++i) {
          slot[1 + i] = e.in[i];
          slot[1 + nw + i] = e.exp[i];
          slot[1 + 2 * nw + i] = e.care[i];
        }
        sp->previous = e;
        sp->have_previous = true;
        ++produced;
      }
      return produced;
    };
    cvm::registry::messenger.signal<cvm::pipe_producer>(location, producer);

    sessions().push_back(std::move(s));
    return 0;
  }

  void cvm_replay_report_word(const char* hier, int index,
                              const svLogicVecVal* w) {
    if (index < 0 || w == nullptr)
      return;
    auto& bits = report_bits()[hier];
    if (bits.size() <= static_cast<std::size_t>(index))
      bits.resize(static_cast<std::size_t>(index) + 1, 0u);
    // bval ignored: the engine is 2-state, the host having resolved X.
    bits[static_cast<std::size_t>(index)] = w->aval;
  }

  void cvm_replay_report(const char* hier, int mismatches, int first_fail_cycle,
                         int cycles, int min_occupancy, int demands) {
    const std::vector<std::uint32_t>& bits = report_bits()[hier];

    if (mismatches != 0) {
      std::string where;
      if (const session* s = lookup_hier(hier)) {
        for (const std::string& name : s->src->failing_bits(bits)) {
          where += where.empty() ? " on " : ", ";
          where += name;
        }
      }
      cvm::log(cvm::ERROR,
               "cvm::replay: {}: {} mismatching bit-cycles over {} cycles, first "
               "at cycle {}{}\n",
               hier, mismatches, cycles, first_fail_cycle, where);
    }

    // How close the transport came to running dry, and how often the design
    // had to ask. A passing run still needs this, or a first hardware run says
    // only pass or fail, not by how much.
    cvm::log(cvm::LOW,
             "cvm::replay: {}: {} cycles, least transport occupancy {}, {} "
             "demands\n",
             hier, cycles, min_occupancy, demands);
  }

} // extern "C"
