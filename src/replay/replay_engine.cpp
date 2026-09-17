// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/replay_engine.hpp"

#include <gflags/gflags.h>

#include "cvm/logger.hpp"
#include "cvm/registry.hpp"

DEFINE_string(cvm_replay_file, "",
              "recorded vector file: either a bare path, or key=path pairs "
              "separated by commas, keyed by topology path");
namespace cvm {
  namespace replay {

    namespace {

      // The waiting caller is a returning DPI with the clock stopped, so
      // release it however the handler ends.
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

    engine::engine(cvm::topology::loc_t loc, unsigned id)
        : loc_(loc), who_(cvm::topology::name(loc)), pipe_(loc, id) {
      auto& m = cvm::registry::messenger;

      m.connect<bind_request>(loc, [this](const bind_request& r) {
        const release _{r.done};
        const int status = bind(r);
        if (r.status != nullptr)
          *r.status = status;
      });

      m.connect<ignore_request>(loc, [this](const ignore_request& r) {
        const release _{r.done};
        const int status = ignore(r);
        if (r.status != nullptr)
          *r.status = status;
      });

      m.connect<load_request>(loc, [this](const load_request& r) {
        const release _{r.done};
        const int status = load(r);
        if (r.status != nullptr)
          *r.status = status;
      });

      m.connect<report_word>(loc, [this](const report_word& w) {
        if (w.index < 0)
          return;
        if (fail_bits_.size() <= static_cast<std::size_t>(w.index))
          fail_bits_.resize(static_cast<std::size_t>(w.index) + 1, 0u);
        fail_bits_[static_cast<std::size_t>(w.index)] = w.bits;
      });

      m.connect<report>(loc, [this](const report& r) {
        const release _{r.done};
        reported_ = true;
        if (r.mismatches != 0) {
          // Empty for an instance that never loaded, so the summary still
          // appears -- by bit index rather than by port name.
          std::string where;
          for (const std::string& name : src_.failing_bits(fail_bits_)) {
            where += where.empty() ? " on " : ", ";
            where += name;
          }
          cvm::log(cvm::ERROR,
                   "Error: cvm::replay: {}: {} mismatching bit-cycles over {} cycles, "
                   "first at cycle {}{}\n",
                   who_, r.mismatches, r.cycles, r.first_fail_cycle, where);
        }
        // A passing run needs this too, or a first hardware run says only pass
        // or fail, not by how much headroom.
        cvm::log(cvm::LOW,
                 "cvm::replay: {}: {} cycles, least transport occupancy {}, {} "
                 "demands\n",
                 who_, r.cycles, r.min_occupancy, r.demands);
      });
    }

    // A recording that was loaded and never replayed to the end is the quiet
    // failure this guards against: the run says nothing and reads as a pass.
    // The design reports at `done`, so its absence is the signal -- the encoder
    // finishing only means the transport swallowed the recording, which a short
    // one does whether or not `enable` ever rose.
    void engine::check() const {
      if (path_.empty() || reported_)
        return;
      cvm::log(cvm::ERROR,
               "Error: cvm::replay: {}: `{}` was loaded but replay never "
               "finished; {} elements reached the transport. Was `enable` "
               "asserted?\n",
               who_, path_, encoder_.produced());
    }

    int engine::bind(const bind_request& r) {
      if (r.name == nullptr || r.width <= 0)
        return -1;
      bindings_.push_back(binding{std::string(r.name),
                                  static_cast<std::size_t>(r.width),
                                  static_cast<std::size_t>(r.bit_offset),
                                  r.is_output});
      return 0;
    }

    int engine::ignore(const ignore_request& r) {
      ignored_.emplace_back(r.name);
      return 0;
    }

    int engine::load(const load_request& r) {
      const std::size_t words_per_element =
          static_cast<std::size_t>(r.elem_words);

      // No recording for this instance. The producer is still installed, and
      // reports end-of-stream on its first call, so the design finishes without
      // ever driving and stays a transparent wire. Closing the transport here
      // instead would race the pipe's own reset, which clears that flag.
      const auto path = cvm::topology::resolve_keyed(FLAGS_cvm_replay_file, loc_);
      if (!path.has_value()) {
        cvm::log(cvm::LOW,
                 "cvm::replay: {}: no recording, so nothing to replay\n", who_);
        install_producer(words_per_element);
        return 0;
      }

      file_.open(*path);
      if (!file_.is_open()) {
        cvm::log(cvm::ERROR, "Error: cvm::replay: cannot open `{}`\n", *path);
        return -1;
      }
      if (!src_.open(file_, *path))
        return -1;

      // A port the recording does not carry is fatal: the spec describes the
      // boundary, so a dump missing one of them is not the recording this
      // interposer was built for.
      for (const binding& b : bindings_) {
        if (src_.bind(b.name, b.width, b.is_output, b.bit_offset) < 0)
          return -1;
      }

      if (!src_.require_all_bound(ignored_))
        return -1;

      if (static_cast<int>(src_.total_bits()) != r.port_bits) {
        cvm::log(cvm::ERROR,
                 "Error: cvm::replay: {}: the bound ports span {} bits but the "
                 "interposer has {}\n",
                 who_, src_.total_bits(), r.port_bits);
        return -1;
      }
      path_ = *path;
      encoder_.start(src_, words_per_element);
      install_producer(words_per_element);
      return 0;
    }

    void engine::install_producer(std::size_t words_per_element) {
      pipe_.producer(
          [this](std::uint32_t* out, std::size_t max_elements) -> std::size_t {
            return produce(out, max_elements);
          },
          words_per_element);
    }

    // The encoder knows nothing of the transport, so closing it and reporting a
    // parse failure are this component's job.
    std::size_t engine::produce(std::uint32_t* out, std::size_t max_elements) {
      const std::size_t produced = encoder_.fill(out, max_elements);
      if (encoder_.finished()) {
        pipe_.close();
        if (!src_.error().empty())
          cvm::log(cvm::ERROR, "Error: cvm::replay: {}: {}\n", who_, src_.error());
      }
      return produced;
    }

  } // namespace replay
} // namespace cvm
