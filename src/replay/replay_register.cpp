// SPDX-FileCopyrightText: © 2026 Tenstorrent USA, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "cvm/registry.hpp"
#include "cvm/replay_engine.hpp"
#include "cvm/topology_defs.hpp"

// Replay owns its transport, so a consumer declares one node of this type and
// passes its location to the generated module. The registration expands against
// the consumer's cvm::static_topology, so replay_register() compiles this unit
// once per topology instead of inside the topology-agnostic //:replay.
REGISTRY_register(cvm::replay::engine, REPLAY, cvm::registry::all)
