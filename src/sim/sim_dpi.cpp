// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/logger.hpp"

namespace {

  int g_errors = 0;

} // namespace

// On request, not at static init, so it cannot race the logger's construction.
extern "C" void cvm_error_count_start() {
  g_errors = 0;
  cvm::set_logger_handler(cvm::ERROR, []() { ++g_errors; });
}

extern "C" int cvm_error_count() { return g_errors; }
