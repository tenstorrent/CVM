// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include <gflags/gflags.h>

#include <atomic>

#include "cvm/logger.hpp"
#include "cvm/pipe.hpp"
#include "cvm/registry.hpp"

// Bare value, or `KEY=value` pairs keyed by topology path. A path covers
// every instance at it: these are rates, not per-instance behaviour.
DEFINE_string(cvm_pipe_credit_every, "1",
              "return credit every Nth element consumed");
DEFINE_string(cvm_pipe_demand_watermark, "0",
              "occupancy at or below which the design asks for elements");
DEFINE_string(cvm_pipe_demand_every, "1",
              "cycles between demands once at or below the watermark");

// One name per formal size, because a DPI function has a single signature.
// Weak, since a design elaborates only the size it needs: the rest resolve to
// null, which also catches a size the generate block never made.
#define CVM_PIPE_SLOTS(X) \
  X(8)                    \
  X(32)                   \
  X(128)                  \
  X(512)                  \
  X(2048)                 \
  X(8192)                 \
  X(32768)

#define CVM_PIPE_DECL_PUSH(N)                                 \
  extern "C" __attribute__((weak)) void cvm_pipe_in_push_##N( \
      unsigned int count, const unsigned int* data_words,     \
      unsigned char is_last);
CVM_PIPE_SLOTS(CVM_PIPE_DECL_PUSH)
#undef CVM_PIPE_DECL_PUSH

// Strong on purpose: a weak reference does not pull a member out of a static
// archive, so without this one nothing in the export object would resolve.
extern "C" void cvm_pipe_in_zero();

namespace {

  decltype(cvm::pipe_reset::push) push_for(unsigned int slot_words) {
#define CVM_PIPE_CASE(N) \
  if (slot_words == N)   \
    return &cvm_pipe_in_push_##N;
    CVM_PIPE_SLOTS(CVM_PIPE_CASE)
#undef CVM_PIPE_CASE
    return nullptr;
  }

  int int_from_keyed(const std::string& flag, cvm::topology::loc_t loc,
                     int fallback) {
    const auto v = cvm::pipe::resolve_keyed(flag, loc);
    if (!v.has_value())
      return fallback;
    try {
      return std::stoi(*v);
    } catch (...) {
      cvm::log(cvm::ERROR, "cvm::pipe({}): cannot parse `{}` as a number\n", loc,
               *v);
      return fallback;
    }
  }

} // namespace

extern "C" {

  int cvm_pipe_credit_every(unsigned int loc) {
    return int_from_keyed(FLAGS_cvm_pipe_credit_every, loc, 1);
  }

  int cvm_pipe_demand_watermark(unsigned int loc) {
    return int_from_keyed(FLAGS_cvm_pipe_demand_watermark, loc, 0);
  }

  int cvm_pipe_demand_every(unsigned int loc) {
    return int_from_keyed(FLAGS_cvm_pipe_demand_every, loc, 1);
  }

  // Every signal goes out at one priority, so this cannot be overtaken by a
  // credit or a demand.
  void cvm_pipe_reset(unsigned int loc, unsigned int depth,
                      unsigned int words_per_element,
                      unsigned int push_slot_words) {
    const auto push = push_for(push_slot_words);
    if (push == nullptr) {
      cvm::log(cvm::ERROR, "cvm::pipe({}): no push export for {} words\n", loc,
               push_slot_words);
      return;
    }
    // wptr_nxt must NOT be reset from RTL: on some emulators that makes it a
    // dual-driver (RTL + DPI) and the DPI push increment is silently dropped.
    // Calling the export from within this import is legal, and the scope is
    // already the caller's, so it needs no callback to get there.
    cvm_pipe_in_zero();
    cvm::registry::messenger.signal_async<cvm::pipe_reset>(
        loc, {depth, words_per_element, push_slot_words, push},
        cvm::messenger::highest_priority);
  }

  void cvm_pipe_credits(unsigned int loc, unsigned int rptr) {
    cvm::registry::messenger.signal_async<cvm::pipe_credits>(
        loc, {rptr}, cvm::messenger::highest_priority);
  }

  // The handler may run on another thread, so the status has to be waited for.
  int cvm_pipe_demand(unsigned int loc, unsigned int rptr) {
    int status = 0;
    std::atomic<bool> done(false);
    cvm::registry::messenger.signal_async<cvm::pipe_demand>(
        loc, {rptr, &status, &done}, cvm::messenger::highest_priority);
    done.wait(false);
    return status;
  }
}
