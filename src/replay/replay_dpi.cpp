// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <gflags/gflags.h>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "cvm/logger.hpp"
#include "cvm/replay.hpp"
#include "svdpi.h"

DEFINE_string(cvm_replay_file, "",
              "recorded vector file: either a bare path, or key=path pairs "
              "separated by commas, keyed by each interposer's HIER parameter");
DEFINE_bool(cvm_replay_strict_x, false,
            "compare recorded X/Z exactly instead of skipping those bits; only "
            "meaningful on a 4-state simulator");
DEFINE_string(cvm_replay_mode, "REPLAY",
              "REPLAY (drive the DUT from the recording) or BYPASS (the "
              "testbench drives; the interposer is a transparent wire)");

namespace {

struct session {
  std::unique_ptr<std::ifstream> file;
  std::unique_ptr<cvm::replay::source> src;
  cvm::replay::replay_vector current;
  // What the interposer observed, pushed a word at a time before check().
  std::vector<cvm::replay::logic_word> observed;
  std::string hier;
};

std::vector<std::unique_ptr<session>>&
sessions() {
  static std::vector<std::unique_ptr<session>> s;
  return s;
}

session*
lookup(int handle) {
  if (handle < 0 || static_cast<std::size_t>(handle) >= sessions().size()) {
    cvm::log(cvm::ERROR, "cvm::replay: invalid handle {}\n", handle);
    return nullptr;
  }
  return sessions()[static_cast<std::size_t>(handle)].get();
}

} // namespace

extern "C" {

int cvm_replay_open(const char* hier, const char* layout) {
  const auto path = cvm::replay::resolve_path(FLAGS_cvm_replay_file, hier);
  if (!path.has_value()) {
    cvm::log(
        cvm::ERROR,
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
  s->src->set_strict_x(FLAGS_cvm_replay_strict_x);
  if (!s->src->open(*s->file, layout))
    return -1;

  sessions().push_back(std::move(s));
  return static_cast<int>(sessions().size()) - 1;
}

// Returns 1 and this vector's absolute recorded time, or 0 at end of stream.
// Absolute rather than a delta so the caller can wait until origin + time and
// not accumulate drift from its own strobe delay.
int cvm_replay_next(int handle, unsigned long long* at) {
  session* s = lookup(handle);
  if (s == nullptr)
    return 0;
  if (!s->src->next(s->current))
    return 0;
  *at = s->current.time;
  return 1;
}

void cvm_replay_word(int handle, int index, svLogicVecVal* out) {
  out->aval = 0;
  out->bval = 0xFFFFFFFFu; // reads as Z if anything goes wrong
  session* s = lookup(handle);
  if (s == nullptr)
    return;
  if (index < 0 || static_cast<std::size_t>(index) >= s->current.value.size()) {
    return;
  }
  static_assert(sizeof(cvm::replay::logic_word) == sizeof(svLogicVecVal),
                "logic_word must stay layout-compatible with svLogicVecVal");
  const cvm::replay::logic_word& w =
      s->current.value[static_cast<std::size_t>(index)];
  out->aval = w.aval;
  out->bval = w.bval;
}

// Pushes one word of what the interposer observed on the DUT boundary.
void cvm_replay_observed(int handle, int index, const svLogicVecVal* w) {
  session* s = lookup(handle);
  if (s == nullptr || w == nullptr)
    return;
  if (index < 0)
    return;
  if (s->observed.size() <= static_cast<std::size_t>(index)) {
    s->observed.resize(static_cast<std::size_t>(index) + 1);
  }
  s->observed[static_cast<std::size_t>(index)].aval = w->aval;
  s->observed[static_cast<std::size_t>(index)].bval = w->bval;
}

// Compares the pushed observation against the current vector. Returns the
// number of mismatching bits, which is also reported through cvm::log.
int cvm_replay_check(int handle, unsigned long long sim_time) {
  session* s = lookup(handle);
  if (s == nullptr)
    return 0;
  return static_cast<int>(s->src->check(s->observed, sim_time, s->hier));
}

// 0 = REPLAY, 1 = BYPASS. Resolved once, at the time origin.
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

// SV formats the message, since that is where the 4-state values live. Note
// this does not fail a test on its own: the testbench must register a handler
// with cvm::set_logger_handler(cvm::ERROR, ...).
void cvm_replay_error(const char* msg) {
  cvm::log(cvm::ERROR, "cvm::replay: {}\n", msg);
}

} // extern "C"
