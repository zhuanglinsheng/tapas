# How Tapas Works

[简体中文](Mechanism_zh.md) | English | [Project Home](../README_en.md)

This document explains how Tapas compiles source into bytecode and how the virtual machine manages state, calls functions, and releases composite values.
For language rules, see the [Language Reference](Syntax_en.md) and [Type System](TypeSystem_en.md); this document describes the current implementation only.

## From Source to Bytecode

Tapas files, Markdown code blocks, command strings, and REPL input all use the same compilation pipeline:

```text
source
  -> document and lossless tokens
  -> AST
  -> names, scopes, and control-flow facts
  -> Type inference and checking
  -> bytecode
```

The frontend first retains immutable source text and its line index, then produces lossless tokens that include whitespace and comments and parses the syntax into an AST.
Semantic analysis builds lexical scopes, declarations, and name references; control-flow analysis records node relationships, enclosing functions, definite returns, and definite initialization; Type analysis performs inference and checking over the same result.
The backend assigns environment and temporary slots and emits bytecode only when the frontend has no errors.

The command-line program and language server share this pipeline, so they no longer maintain separate syntax or Type rules.
See the [compiler structure](../src/compile/README_en.md) for source directories and dependency boundaries.

The compiled bytecode wrapper contains:

- a list of 32-bit instructions;
- integer, floating-point, and string constant tables;
- a file and source location for each instruction;
- maximum capacities for environment slots, temporary slots, and the runtime stack.

The compiler has already assigned ordinary names to slots, so the virtual machine does not look them up by source name during variable access.
A wrapper can be executed immediately or saved as a `.tapc` file and loaded later; see [Usage](Usage_en.md#compile-and-run-bytecode) for the corresponding commands.

## Runtime State

A library is the root environment of a program.
When a Tapas function is called, the virtual machine creates a call environment beneath the parent environment captured when that function was defined.
Values that must survive across expressions occupy environment slots, intermediate expression results use the runtime stack, local temporary values use separate temporary slots, and the virtual machine keeps a dedicated return-value location.

For example:

```tapas
var mechanism_total = 0
mechanism_total = 1 + 2
```

The first statement creates the environment slot for `mechanism_total` and writes the integer constant `0` into it.
The second reads `1` and `2` from the constant table, performs the addition on the runtime stack, and moves the result into the existing slot.
The stack values used by that computation are cleared when the assignment finishes.

## Instruction Execution and Function Calls

The virtual machine reads instructions in program-counter order and decodes their opcodes and operands in one interpreter loop; jump instructions update the program counter directly.

Each Tapas function call has its own frame containing the program counter, return location, call environment, temporary slots, runtime stack, and loop cursors.
The virtual machine switches frames inside the same interpreter loop instead of recursively entering a new C interpreter function for every Tapas call.
When a function returns, the values held by its frame are cleared, but allocated storage is retained for later calls at the same depth.

An ordinary call writes arguments into reserved parameter slots as it enters the function.
Only a dynamic parameter list must read directly from the caller's runtime stack during the call.
Calling the same function again at the same depth can reuse a validated frame layout; calling another function still checks the parent environment, argument count, and storage capacities.

Direct tail recursion in the form `return this (...)` does not increase call depth.
The virtual machine preserves the new arguments, clears and reuses the current frame, and continues from the function entry.

A function value may refer to the parent environment at its definition site.
When a returned closure still needs the current call environment, the runtime retains that environment so it does not depend on a frame that may later be reused.
Loop cursors also belong to individual frames, so recursion through the same loop instruction cannot disturb an outer iteration.

## Instruction Caches

Bytecode records operations only; it neither records value categories observed during one execution nor rewrites itself at runtime.
The virtual machine maintains a separate cache for each piece of executing code and addresses cache entries by instruction position.

Loops, indexing, and function calls use a generic path on first execution.
After the same position repeatedly sees the same value category or function, its cache can route later executions directly to the matching implementation.
Values still undergo the required checks, and a different case falls back to generic dispatch without changing the original instruction.

Index caches can directly handle one-integer indexing for List and String and two-integer indexing for RealArray and BoolArray.
The dense-array path computes the element position directly while retaining negative-index, bounds, and write-Type checks; slices and different value categories fall back to the generic path.

Caches are not written to `.tapc` files and do not belong to call frames.
A list loop's current position, by contrast, belongs to its call frame and uses a cache-assigned slot, so separate virtual machines and recursive calls never share loop state.

## Function Declaration Metadata

Functions hold immutable, reference-counted `tfunction_metadata`: parameter names,
actual Types, return Type, and variadic status. Without value parameters, `OP_FUNCMETA` decodes and validates
the bytecode declaration once per instruction cache entry. Closures created at
that code position and function copies share metadata; functions and caches own
independent references.

With `InstanceOf[R]`, `OP_BINDTYPE` resolves R in the defining environment and
retains the actual Rule object. `OP_FUNCMETA` creates fresh bound metadata per
closure rather than caching that identity at the code position. `OP_CHECKTYPE`
checks declaration initialization; call frames check identity-bearing arguments
and results; `OP_RULETYPE` installs bound Rule parameter Types. Bytecode carries
rebindable name templates, not portable serialized runtime identities.

`src/stdlib/function_metadata.c` registers native parameter descriptors and Type
factories, including optional/variadic flags, independently of presentation-only
`detail` text. Native metadata is built on first reflection and cached on the
function. Tests verify coverage of all standard-library functions. The extension
ABI is unchanged; display documentation is still maintained separately.

`parameters` projects fresh name/Type pairs without reparsing signatures or running
functions. Strings remain the bytecode transport, not the runtime representation.

## Lifetime of Composite Values

String, List, Dictionary, Function, and other composite values share the `tcompo_v` base and use reference counting for lifetime management.
Environment slots, collection elements, the runtime stack, and return values may all hold composite values.

Copying an ownership relationship increments the reference count; overwriting or clearing its location decrements it.
Moving a value from the runtime stack into an environment slot or return location can transfer the existing ownership directly instead of incrementing and then decrementing it.
Int, Float, and Bool hold no composite value, so popping them only moves the top-of-stack position.

When a reference count reaches zero, the composite value releases itself through its vtable.
List, Pair, and Dictionary also release the elements they own, while a dense array directly owns its scalar storage.

A list slice determines its final length before allocating its result and then copies elements contiguously, avoiding repeated capacity checks and growth through item-by-item appends.
Short String data is stored inside `tstring`; a separate buffer is allocated only when that internal capacity is exceeded.

Reference counting cannot discover unreachable reference cycles.
The current runtime has no additional tracing cycle collector, so programs should avoid forming container cycles that are no longer used.

## Bytecode Format

Tapas bytecode consists of abstract virtual-machine instructions, not physical CPU instructions.
Each instruction is a 32-bit unsigned integer whose low six bits hold the opcode; the remaining bits are interpreted as one, two, or three operands according to that opcode.

Instructions broadly cover:

- stack, slot, and constant operations;
- jumps, conditional branches, and loops;
- function construction, ordinary calls, static calls, closure calls, and tail calls;
- Type forward declarations and definitions;
- Rule construction, Conditions, and child Requirements;
- indexed reads, indexed writes, and imports;
- arithmetic, comparison, logical, and matrix operations.

Opcodes evolve with the implementation.
The complete enumeration is defined by [`tins`](../include/tapas/basic_defs/tbasis.h), while the [bytecode interface](../include/tapas/basic_defs/tbycs.h) defines operand layouts and wrapper structures.
Keeping the volatile instruction-by-instruction list in source prevents the documentation from reporting an obsolete count or omitting newly added instructions.

## Checking Implication Antecedents

`implies` adds no opcode. The compiler uses a private antecedent mode of
`OP_RULECOND` to retain the original Bool/RuleInstance, check instance
satisfaction, and leave a Bool for existing conditional jumps. Ordinary
Conditions still require Bool.

Source metadata distinguishes the raw antecedent from its cached truth for that
check. The default checker does not evaluate it twice or merge its violations
into the enclosing result. Public IR retains the original Term and Type.
Newly generated bytecode requires the matching runtime; older runtimes do not
understand this private mode. TPIR3 is a dynamic IR format, not the bytecode format.

## Executing Rule negation

Ordinary Bool `not` still lowers to conditional jumps. Within a Rule's own expressions, the appended `OP_RULENOT` preserves the operand, negates Bool directly, or checks a RuleInstance once non-fatally and negates `passed`. The compiler chooses this lexically; calling an ordinary function from a checker does not change its behavior.

Source checkers record negation operands and results in a separate per-check buffer, saved and restored across nested checks. This preserves Condition/Implication record ordering and avoids repeating child checks when assembling results. Source metadata retains explicit Not Terms and operands, while the dynamic evaluator interprets Not directly. RuleIR containing Not uses TPIR4 independently of the VM bytecode format; the new opcode requires its matching runtime.

## Rule logical composition and bare instance items

Rule-local `and` and `or` use `OP_RULETRUTH` for reached Bool/RuleInstance operands and existing conditional jumps for short-circuiting. Source traces retain operands and the final Bool without forcing skipped branches for reflection. Dynamic And/Or nodes implement the same ordered short-circuit semantics under TPIR5.

`OP_RULEITEM` records a Bool condition or retains an instance as a child obligation. Statically known instance items have Requirement IR; dynamic instances also retain violation paths. The `require` keyword and grammar are removed. Legacy `OP_RULEREQ` remains readable, while newly generated bytecode needs its matching runtime. Internal Requirements and their builder API remain supported.

## Inspecting Bytecode

The sample program is [`examples/test_bycodes.tap`](examples/test_bycodes.tap).
From the repository root, the following command compiles that file and displays the generated bytecode:

```sh
build/bin/tapas -cr docs/examples/test_bycodes.tap
```

The C session API provides the same operation:

```c
#include "tapas/tsession.h"

int main(void)
{
    tsession *session = tsession_new();
    const char *source = "docs/examples/test_bycodes.tap";

    tsession_compile_file(session, source, 1);
    tsession_show_bycodes(session, source);
    tsession_free(session);
    return 0;
}
```

See [C Interaction](Foreign_en.md) for the declarations and embedding workflow.

Inside Tapas, `__binary__()` displays the current library's bytecode.
When passed a Library or Function, it displays the bytecode or instruction range associated with that object.
This function is an implementation-inspection aid, not a stable program-output interface.
