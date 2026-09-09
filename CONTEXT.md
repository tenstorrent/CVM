# cvm

`cvm` is a library of utilities shared by C++ and SystemVerilog testbenches. This file is the
project glossary: the canonical name for each domain concept, and the alternatives to avoid. It
holds no implementation detail.

## Language

### Cycle replay

**EVCD**:
An Extended Value Change Dump, as produced by Verilog's `$dumpports`. Unlike plain VCD, each
recorded port value encodes which side of the port is driving, which is what makes a dump replayable.
_Avoid_: VCD (a different, non-directional format), waveform, trace

**Cycle**:
The unit of replay time: one `#1` in a recording is one clock. A recording is therefore already
cycle-indexed, and a recorded timestamp *is* a cycle number.
_Avoid_: time, timestamp, tick, step

**Cycle update**:
Everything replay changes for one cycle: the stimulus to drive, the expectation for that cycle, and
the care mask over it. What replay puts inside one transport element.
_Avoid_: vector, record, frame, sample, snapshot. **`Vector` is retired**: it meant "one timestamped
set of port values, the unit replay consumes", and neither half is true now.

**Expectation**:
The outputs a recording holds for a cycle. A recording pairs a cycle's inputs with the outputs
*standing during* that cycle, never with the outputs those inputs will cause.
_Avoid_: expected value, golden, reference

**Care mask**:
Which expectation bits are compared. Clear for anything the recording never wrote and for anything
it recorded as unknown, since an emulator has no X to compare against.
_Avoid_: valid mask, enable, don't-care

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

**Cycle origin**:
The cycle on which replay starts, when `enable` rises. Every cycle number in a recording is counted
from it. Renamed from `Time origin`, which described a simulation time that no longer exists.
_Avoid_: t0, start time, time zero, epoch (epoch means something else here)

**Conformance**:
The property that a dump matches a port spec: every declared port present, with the expected width
and consistent drivers. Checked once, before replay begins.
_Avoid_: validation, compatibility, schema check

### Transport

The transport is a layer below replay and knows nothing about it. Replay's words must not appear
here: an element's content is opaque by design, which is what keeps the transport reusable.

**Element**:
The transport's unit of delivery. One element is available per cycle, so everything for a cycle
arrives together.
_Avoid_: record, vector, beat, entry

**Payload**:
An element's content: `WIDTH` bits whose meaning belongs entirely to the consumer. The transport
never interprets it.
_Avoid_: data, message, fields

**Epoch**:
One batch of elements, pushed as a unit. The host does not send the next until the HDL has confirmed
the last, which is what makes the protocol safe without the host guessing at free space.
_Avoid_: burst, chunk, window, frame

**Headroom**:
Elements still held when the next epoch is requested, sized to cover how long a push takes to land.
Its whole job is that the request goes out early enough.
_Avoid_: threshold, margin, low water mark

**Relief fetch**:
The fallback that returns elements *inline* when a push has not landed in time, making progress
guaranteed rather than likely. The only steady-state call that stops the clock, which is why it is a
fallback.
_Avoid_: flush (means draining the callback queue), bypass (means `Bypass mode`), demand fetch,
pull
