# cvm

This is a library for utilities used in C++ and SV testbenches. There are examples of how to use these under the `test` directory.

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

The registry also provides a `build` and `shutdown` function to better manage test phasing. For example, in [rv_tester](https://aus-gitlab.local.tenstorrent.com/riscv/dv/rv_tester/-/blob/master/rv_tester.sv) a C++ class can assume that its constructor will be invoked prior to a test and its destructor will be invoked at the end of a test. Generally, the C++ class should setup its `messenger` connections in its constructor.

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

## FAQ

+ Why can't I call coroutines (`task<T>`) from normal functions?

This is due to function coloring. There's an article about this here https://journal.stuffwithstuff.com/2015/02/01/what-color-is-your-function/. If you really want to do this, check out the `fork` function.
