# cvm

This is a library for utilities used in C++ and SV testbenches. There are examples of how to use these under the `test` directory.

## Getting Started

`cvm` builds with [Bazel](https://bazel.build/) and is developed inside a
container image (`ghcr.io/tenstorrent/cvm`, defined by the `Containerfile` at
the repo root) that provides the required toolchain (Bazel, clang 20, Python 3).

```sh
# Build everything
bazel build //...

# Run the unit tests under test/
bazel test //test/...
```

The `test/` directory contains runnable examples for each utility described
below.

## plusargs

Portable way to get plusargs from both C++ and SV using [gflags](https://gflags.github.io/gflags/).

```cpp
// C++
DEFINE_bool(example)
//...
if (FLAGS_example)
//...
```

```verilog
// SV
bit example = cvm_plusargs::get_bool("FLAGS_example");
```

## logger

C++ logging (printf) tool while being able to control verbosity with the `+cvm_verbosity` plusarg. In increasing verbosity,

1. ERROR
2. NONE
3. LOW
4. MEDIUM
5. HIGH
6. FULL
7. DEBUG

These can also be connected to a callback function to perform a task if a corresponding verbosity level print is used. For example, the testbench can use an ERROR condition to terminate the test instead of using assert statements.

Note: The output log file doesn't get created until the first write to the file. 

## bitmanip

Provides functions to manipulate C++ primitive types as well as std::bitset (used by packet gen, explained below).

`slice` is capable of returning a value of a bit range defined by msb/lsb from **bitset/primitive to primitive** or **bitset to bitset**. It can also convert a bitset/primitive to a vector of primitives.

## topology

Useful way of declaring modules as a design topology in a yml file and reuse at compile time. We use this for the rest of the utilities to uniquely identify modules in a topology using a location id `loc_t`.

```
top:
  example:
    count: 1
    type:
      - something
    attrs:
      att: 10
```

A topology definition requires the following:

+ overlaying (single) `top` module
+ each node should have a `count` and `type` field denoting:
  + `count`: number of this module at this level of hierarchy
  + `type`: the "type" of a module. useful for C++ registry instantiation explained below

optionally,

+ `attrs`: sequence of key, value pairs used to describe a module. For example, if RTL had a hard-coded parameter that a C++ testbench should know about.

The locations of other modules can be discovered using `get_from_type` or `get_from_hierarchy`, which will return a vector of other module locations determined by either the `type` specified or the full `hierarchy` path. We also obtain module attributes through `attr` on a per-location basis (usually a module's own location).

On the SV side, a module can obtain its own location using `cvm_topology::get_location`, which only accepts a full hierarchical path determined by `topology_gen`'s generated SV package `topology_pkg`. This package definition should be passed down to children nodes at compile time using the `TOPOLOGY` macro define.

The `topology_gen` rule can accept a list of such yml files where they're all concatenated. To take advantage of this, the user can use [YAML anchors](https://yaml.org/spec/1.2/spec.html#id2765878) and instantiate modules/attributes to describe the design in a modular way.

## registry

This is the primary way C++ classes should instantiate themselves to be topology agnostic. The macro `REGISTRY_register` will instantiate the C++ class for all modules defined in a topology path or type (`cvm::registry::all`), or for a specific id within that path or module type. Needing to specify a specific module id is not recommended.

The registry also provides a `build` and `shutdown` function to better manage test phasing. For example, in an upstream SV testbench, a C++ class can assume that its constructor will be invoked prior to a test and its destructor will be invoked at the end of a test. The testbench may also do location-based resets for finer-grained control of build/shutdown functions (TODO: hierarchical-path based resets). Generally, the C++ class should setup its `messenger` connections in its constructor.

### messenger

Used to broadcast information to/from C++ classes. There are two steps to this, a subscriber should use

`cvm::registry::messenger.connect<transaction_type_t>(location, some_lambda);`

to indicate they are listening at location id `location` for the transactions `transaction_type_t` (just any C++ type, generally a struct), and to execute some kind of function upon receiving this message. The lambda should be able to accept the transaction as a const reference (e.g. `const transaction_type_t& transaction`) where presumably the listener will do some processing based on the message.

The user can optionally add a lightweight filter on a transaction to avoid spurious wakeups.

The publisher does something similar, but passes the actual message

`cvm::registry:messenger.signal<transaction_type_t>(location, transaction_type_t{})`

where they can assume all subscribers will receive this message, but not necessarily know which ones.

#### coroutines

Alternatively, the user can use coroutines as listeners like so

`cvm::registry::messenger.fork(some cvm::messenger::task, args)`

This can be though of as the equivalent of "spawning" a thread which will automatically block execution with `co_await` statements. Along with this, messenger provides the concept of `waits` with both

`wait<transaction_type_t>(location)` which waits for the next occurence of a message and

`wait<transaction_type_t>(channel)` which waits on a messenger-managed queue of transactions. The channel is automatically populated with any new transactions matching the transaction type even when the function is suspended (new, as in after channel creation). Channels are created with `cvm::registry::messenger.channel<transaction_type_t>(location)` and the user should capture the return value to pass to the `wait` function. Similar to normal `connect`s, the channel can also consume a filter on transaction `co_awaits`. This is more efficient since we can avoid extra `resume` -> `await` operations if a transaction does not apply to a particular coroutine. The filter can be thought of as a C++ std::views, which does not modify the underlying contents of the channel.

WARNING: don't use a capture list in lambda coroutines, this is dangerous - https://clang.llvm.org/extra/clang-tidy/checks/cppcoreguidelines/avoid-capturing-lambda-coroutines.html

some guides on C++ coroutines,
+ https://en.cppreference.com/w/cpp/language/coroutines
+ https://itnext.io/c-20-coroutines-complete-guide-7c3fc08db89d
+ https://lewissbaker.github.io/2017/11/17/understanding-operator-co-await
+ https://lewissbaker.github.io/2022/08/27/understanding-the-compiler-transform

#### Procedure Calls

The messenger supports a system similar to a Remote Procedure Call. 

`CVM_MESSENGER_procedure_call(name, func_type)`

Create a name and function type for a procedure call. The function type represents the return type and argument types of the function, like `int (int, int)` for a function that would have two arguments of type int and return another int. This must be done in a header and included by any files that use procedure or call for that name type. 

`cvm::registry::messenger.procedure<name>(cvm::topology::loc_t loc, func_type listener)`

Register a `listener` function with the specified name to a location. 

`cvm::registry::messenger.call<name>(cvm::topology::loc_t loc, Args... args)`

Call a registered function with a specified name. This will return the return value of the registered listener function. 


### callbacks

This is used for issuing callbacks from C++ to SV through DPI, by passing the relevant DPI function's `svScope` as well as a `std::function` with `void()` type.

`cvm::registry::callbacks.push(scope, [arg]() { dpi_function(arg.a, arg.b); });`

## packet gen

Unified way to issue function calls like `callbacks` from C++ -> SV, but for SV -> C++. The user defines the "packets" in `test/packet_gen/transactions.yml`, the top level represents "ports" which is a useful concept for when there is a single yml file for multiple transaction types that needs to be shared with multiple modules. `packet_gen` itself generates boilerplate for C++ and SV code. These are accessible in SV using

+ `packet_gen_name_DOMAIN` - instantiates an SV module which issues callbacks as well as all packet `logic` signals
+ `packet_gen_name_OUTPUT_port` - macro to indicate output source of a packet (used within a module which doesn't have the `packet_gen` module instantiation itself)
+ `packet_gen_name_SOURCE_port` - macro to connect packet `logic` signal to child module ports

as well as the packet types defined in the yml itself in both SV and C++.

A packet needs a `valid` field to be set to indicate when to issue a message on a posedge clock (call DPI function) as well as the source's location since this relies on topology/messenger. To work properly, the C++ class listening on this SV message will have used a `cvm::registry::messenger.connect` on the `packet_gen` transaction type.

It's possible to have multiple `packet_gen` rules in a build, but this also means the relevant DPI function calls will be in different `always` blocks, which may hurt reproducibility.

### Qualify
A field in a packet can have a `qualify`. The field within the packet will only be sent from SV to C++ when the qualify term is true. If not true, the C++ struct will have zeros for those fields.

For simple qualify terms, the name of the signal in the packet can be used. For more complex terms, a string can be passed, with `{data}` as the placeholder for the packet path.

For now, fields using the same qualify should be contiguous. This requirement may be relaxed in the future if desired. However the output verilog will be more optimal if they are contiguous.

### Examples

```
    fields:
        dummy:
            width: 1 # field with width of 1
        dummy2:
            width: [2, 32] # field with two variants of width 2 and 32. If this is used, all fields within a packet need the same amount of variants. Useful for parameterizing packets.
        dummy3:
            width: [[2, 2, 4]] # multi-dimensional field of 2x2, each with width of 4. This can be mixed with variants. 
```

## replay (experimental)

Replays a recorded vector stream against an arbitrary Verilog module: drives the
module's inputs from the recorded timeline and checks its outputs against the
recorded values. Useful for turning a full-chip capture into a block-level
regression, or for reproducing a failure without the surrounding environment.

The recording is an [EVCD](https://en.wikipedia.org/wiki/Value_change_dump)
(`$dumpports`), read as cycles: at cycle *k* the dump holds the inputs for cycle
*k* beside the outputs standing *during* it. It is a runtime input, not a build
input, so one build replays any number of conforming recordings.

### Generating the interposer

Point the rule at the DUT's own `verilog_library` and it derives the port spec
with slang:

```python
load("@cvm//:defs.bzl", "replay")

replay(
    name = "alu_replay",
    dut = "alu",
    dut_lib = ":alu_sv",
    clock = "clk",
)
```

| attribute | |
|---|---|
| `dut_lib` | the DUT's `verilog_library`. Carries its sources, its include dirs and its package closure, and is added to the generated library's `deps` -- which it must be, or the interposer elaborates before the packages its port types name. |
| `dut` | the module to replay. |
| `clock` | the DUT's clock port. Human input by necessity: a cycle-indexed recording samples once per cycle, so a clock reads as a constant in it and the dump cannot say which port is special. Never replayed. |
| `exclude` | ports to leave unreplayed -- one on a second clock domain, which no cycle-indexed recording describes. Also how a whole-hierarchy `$dumpports` is made usable, since a dump port nothing binds is fatal. |
| `slang_defines` | read only so slang can pick a branch to elaborate. The spec must come out identical whichever way these are set; `//test/replay:sh_spec_is_define_independent` is that check. |
| `srcs` | a hand-written spec instead of `dut_lib`, for a DUT slang cannot see. Exactly one of the two. |
| `topology` | resolves `${A.B.C}` interpolation of widths and depths inside a hand-written spec. Unrelated to the topology replay needs at runtime. |

### The spec

The same format either way, so a generated spec and a hand-written one are read
by one parser. Everything in it is a SystemVerilog *expression*, never a resolved
number: the interposer re-declares each port with the DUT's own type and measures
it with `$bits`, so **one generated interposer holds for every parameterization
and every define setting of the DUT**. A resolved width in a spec has silently
stopped describing anything but the configuration it was taken from.

```yaml
alu_replay:                 # = the generated interposer's module name
  dut: alu
  clock: clk
  imports: [alu_pkg]        # emitted in the interposer's header, where port
                            # types resolve; a body import is too late
  parameters:               # with default *expressions*, never values
    LANES: { type: int, default: "alu_pkg::LANES" }
  localparams: |            # widths the DUT keeps private, restated verbatim
    localparam int C_C = 1;
  exclude: [tck]
  ports:
    rst_n:    { dir: in,    width: 1 }                        # a literal, if you know it
    cmd:      { dir: in,    type: "alu_pkg::bundle_t" }       # a struct
    mat:      { dir: in,    type: "logic [alu_pkg::LANES-1:0][3:0]" }
    sel:      { dir: in,    type: "logic [$clog2(alu_pkg::LANES*4)-1:0]" }
    bus:      { dir: inout, type: "wire [2:0]" }              # must name a net
    resp:     { dir: out,   type: "alu_pkg::lane_t" }
    gate:     { dir: in,    width: 1, when: [FEAT_GATE] }     # conditional in the DUT
    no_gate:  { dir: in,    width: 1, when: ["!FEAT_GATE"] }  # ...its else branch
    dbg:      { dir: in,    width: 4, dump_name: dbg_bus }    # dump names it differently
```

`dir` is **always** from the DUT's perspective: `in` means driven *into* the DUT.
Give exactly one of `type` (the declaration as written) or `width` (a literal).
`when` is the chain of conditions the DUT declares the port under, emitted as
nested `` `ifdef ``s -- or `` `ifndef `` for an entry written `!COND` -- so the
interposer adapts to defines exactly as the DUT does. Defines are never passed to
the generator.

There is no switch for what to check or what to drive: every input is driven and
every output checked, and the recording decides the rest. An output the dump never
wrote, or wrote as X, is simply not compared.

### Conformance

Both directions are fatal, because both mean the recording is not of this module:

+ a port in the spec that the recording lacks, or disagrees on the width of
+ a port the recording carries that nothing binds -- the clock and `exclude` aside

The second is what stops a spec that has drifted from its DUT narrowing the test
in silence.

### Wiring it in

Replay is a registry component, so it needs a [topology](#topology): declare a
node of type `replay` and pass its location to the generated module.

The generated module is an interposer: the DUT's IO flows through it, so every
port appears on both sides -- `*_outer` towards whatever the DUT sits in and
`*_inner` towards the DUT. The names are positional on purpose: the same
interposer goes inside a design, where "tb" would be wrong. It never instantiates the DUT and never calls `$finish`, so it drops into a
larger testbench.

```systemverilog
alu_replay #(.LOCATION(cvm_topology_gen::get_location(topo.TOP.REPLAY.ID, 0))) u_replay (
    .clk(clk), .reset_n(reset_n), .enable(enable), .done(done),
    .rst_n_outer(rst_n_outer), .rst_n_inner(rst_n_inner),
    .result_inner(result_inner), .result_outer(result_outer),
    .bus(bus), /* ... */ );
alu u_dut (.clk(clk), .rst_n(rst_n_inner), .bus(bus), /* ... */ );
```

An `inout` is the exception: it is one net, so it gets **one** port, and the port
connection is what joins the testbench to the DUT. The interposer drives the bits
the recording says the outside had and releases the rest, which means it does not
isolate an inout the way it isolates an input -- a testbench driving one during
replay contends with the recording instead of being overridden.

`enable` is the only control over who drives the DUT: until it rises, and after
replay finishes, the interposer is a transparent wire. `enable`'s rising edge is
the time origin, so a testbench can initialise first and then hand over. An
instance with no recording configured reports `done` without ever driving, so
leaving one out is how you bypass it.

### Replaying a DUT buried in a larger design

The above needs the instantiation site to be a testbench. For a block inside a
chip, `replay_bind` generates a pair instead:

```python
load("@cvm//:defs.bzl", "replay_bind")

replay_bind(
    name = "alu_interposer",
    dut = "alu",
    dut_lib = "//path/to:alu_sv",
    clock = "clk",
)
```

`<name>_sv` is the interposer. It sits **beside** the DUT rather than around it,
so the DUT's instance keeps its name and therefore its hierarchical path -- what
every waveform script, coverage database and constraint file depends on. It holds
nothing of replay: no DPI, no transport, no cvm package, so it is synthesized with
the design. One `ifdef` sets its `REPLAY_ENABLE` parameter and the parameter
decides the rest, so with replay off it is wires.

`<name>_bound_sv` is everything else -- the boundary arithmetic, the host calls,
the engine. A testbench binds it in, and that is what makes the testbench the
owner of `LOCATION`, `reset_n`, `enable` and `done`: a bind takes parameters and
ports, and nothing reaching into a hierarchy can. A parameter cannot be set
hierarchically at all (IEEE 1800 6.20.2 -- parameters build the hierarchy, so
they cannot be read out of it), and `defparam` reaches only one level.

The design instantiates the interposer unconditionally, beside the DUT, with
intermediate nets. No `ifdef` and no generate:

```systemverilog
    wire [3:0] opa_i;
    wire [4:0] result_i;

    alu_interposer u_alu_replay (
        .clk(clk),
        .opa_outer(opa), .opa_inner(opa_i),
        .result_inner(result_i), .result_outer(result),
        .bus(bus), /* ... */ );

    alu u_alu (.clk(clk), .opa(opa_i), .result(result_i), .bus(bus), /* ... */ );
```

An `inout` is **not** repointed. It stays one net, tapped by the interposer, so
with replay off there is nothing between the design and the block at all. Two
isolated sides are not available: `alias` on a port and the `tran` primitives are
both rejected, so one net is the only form -- and during replay a design driving
that net contends with the recording rather than being overridden.

The testbench's whole side is one statement. `.*` fills the boundary by name,
which works because both modules come from one spec, and it carries conditional
ports for free since both declare them under the same `ifdef`:

```systemverilog
bind alu_interposer alu_interposer_bound #(
    .LOCATION(cvm_topology_gen::get_location(topo.TOP.REPLAY.ID, 0))
) u_cvm_replay (
    .reset_n (tb_reset_n),
    .enable  (tb_enable),
    .done    (tb_done),
    .*
);
```

Put the bind under the same define that turns `REPLAY_ENABLE` on, so the two
cannot disagree. If they do it is loud either way: an interposer with nothing
bound drives X through its mux, and a bind with the interposer off never
finishes. Do not instantiate one interposer module at two sites -- both binds
would land on one `LOCATION` and the two transports would collide.

Runtime plusargs, so the vector file needs no recompile:

+ `+cvm_replay_file=<path>`, or `+cvm_replay_file=<path>=<file>,...` keyed by topology
  path, e.g. `TOP.REPLAY=dump.evcd`

## FAQ

+ Why can't I call coroutines (`task<T>`) from normal functions?

This is due to function coloring. There's an article about this here https://journal.stuffwithstuff.com/2015/02/01/what-color-is-your-function/. If you really want to do this, check out the `fork` function.

## Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) for
how to build, test, and submit changes, and note that this project follows the
[Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md). To report a security
vulnerability, follow the process in [SECURITY.md](SECURITY.md).

## License

Source code in this repository is licensed under the Apache License, Version 2.0
(see [LICENSE](LICENSE)). Documentation is licensed under CC-BY-4.0 (see
[LICENSE-DOCS](LICENSE-DOCS)). Bundled third-party files retain their own
licenses, catalogued in the [LICENSES/](LICENSES) directory and attributed in
[NOTICE](NOTICE).

Note that while this software assists in programming Tenstorrent products,
making, using, or selling hardware, models, or IP may require the license of
rights from Tenstorrent or others — see
[LICENSE_understanding.txt](LICENSE_understanding.txt).

This repository is [REUSE](https://reuse.software) compliant; per-file license
and copyright information is available via inline SPDX headers and
[REUSE.toml](REUSE.toml).
