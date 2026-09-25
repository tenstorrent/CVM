// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

// Stand-ins for simulator-provided VPI/DPI runtime symbols so the topology
// tables, whose CcInfo carries the registry, can link into a plain host
// unit-test binary.
extern "C" {
int vpi_get_vlog_info(void*) { return 0; }
void* svGetScope() { return nullptr; }
}
