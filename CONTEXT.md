# cvm

`cvm` is a library of utilities shared by C++ and SystemVerilog testbenches. This file is the
project glossary: the canonical name for each domain concept, and the alternatives to avoid. It
holds no implementation detail.

## Language

### Vector replay

**EVCD**:
An Extended Value Change Dump, as produced by Verilog's `$dumpports`. Unlike plain VCD, each
recorded port value encodes which side of the port is driving, which is what makes a dump replayable.
_Avoid_: VCD (a different, non-directional format), waveform, trace

**Vector**:
One timestamped set of port values taken from a dump. The unit that replay consumes.
_Avoid_: step, frame, sample, snapshot

**Port spec**:
The checked-in YAML declaring a module's ports, their widths, and their directions. It is the source
of truth: generated code follows it, and a dump is validated against it.
_Avoid_: config, manifest, port map, descriptor

**Interposer**:
The generated SystemVerilog module that a DUT's IO flows through, carrying both sides of every port
so a testbench connects one side and the DUT the other.
_Avoid_: wrapper, shim, harness, adapter, bridge

**DUT side / TB side**:
The two faces of an interposer for a single DUT port, suffixed `_dut` and `_tb`. The DUT side faces
the module under replay; the TB side faces the surrounding testbench.
_Avoid_: inner/outer, near/far, upstream/downstream

**Direction**:
Always stated from the DUT's perspective: an `in` port is driven *into* the DUT, whichever side of
the interposer it appears on. Never stated relative to the interposer or to the dump.
_Avoid_: input/output unqualified, source/sink

**Replay mode**:
The mode in which an interposer drives the DUT's inputs from a dump.
_Avoid_: active, drive mode, playback

**Bypass mode**:
The mode in which the surrounding testbench drives the DUT's inputs and the interposer is
transparent.
_Avoid_: passive, off, disabled, passthrough mode

**Time origin**:
The simulation time at which replay starts. Every timestamp in a dump is applied relative to it, not
as an absolute simulation time.
_Avoid_: t0, start time, time zero, epoch

**Conformance**:
The property that a dump matches a port spec: every declared port present, with the expected width
and consistent drivers. Checked once, before replay begins.
_Avoid_: validation, compatibility, schema check
