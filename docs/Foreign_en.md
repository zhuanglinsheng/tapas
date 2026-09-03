# Tapas C Interaction

[简体中文](Foreign_zh.md) | English | [Project Home](../README_en.md)

This document describes how to call Tapas code from C and how to expose C
functions to Tapas scripts.

The public C API is declared in the headers under `include/tapas`. Most users
only need `tapas/tapas.h`, which includes the session API and the runtime value
types.



## Calling Tapas Scripts From C

Tapas source files are text files with the suffix `.tap`. A C program can run a
Tapas file in either of two ways:

- Compile a `.tap` file to a `.tapc` bytecode file, then evaluate the bytecode.
- Execute a `.tap` file directly without keeping a `.tapc` file.

The main session functions are:

```c
tsession *tsession_new(void);
void tsession_free(tsession *sess);

void tsession_compile_file(tsession *sess, const char *file, int interactive);
void tsession_eval_bycodes(tsession *sess, const char *file);
void tsession_execute_file(tsession *sess, const char *file, int interactive);
void tsession_execute_str(tsession *sess, const char *str, int interactive);
```

For example, suppose `test_calling.tap` contains:

```tapas
function abs(x)
{
    if (x >= 0) {
        return x
    } else {
        return -x
    }
}

abs(-2).print()
```
<pre class='Tapas-Return'>
2
</pre>

The following C program compiles and executes it:

```c
#include "tapas/tapas.h"

int main(void)
{
    tsession *sess = tsession_new();

    tsession_compile_file(sess, "test_calling.tap", 1);
    tsession_eval_bycodes(sess, "test_calling.tapc");

    tsession_free(sess);
    return 0;
}
```

If the bytecode file does not need to be saved, call
`tsession_execute_file` instead:

```c
#include "tapas/tapas.h"

int main(void)
{
    tsession *sess = tsession_new();

    tsession_execute_file(sess, "test_calling.tap", 1);

    tsession_free(sess);
    return 0;
}
```

Tapas code can also be executed from a C string:

```c
#include "tapas/tapas.h"

int main(void)
{
    tsession *sess = tsession_new();

    tsession_execute_str(sess, "print(1 + 2)", 1);

    tsession_free(sess);
    return 0;
}
```



## Extending Tapas With C Functions

C functions can be exposed to Tapas by wrapping them as Tapas callable values
and adding them to the current session library.

The function pointer type is:

```c
typedef void (*genf_t)(tobj *params, uint_regs len, tobj *vre);
```

The parameters are:

- `params`: an array of Tapas values passed by the script.
- `len`: the number of arguments.
- `vre`: the output value that receives the function result.

Use a native extension descriptor so the implementation, name, Tapas Type,
and argument range stay together:

```c
static const textension_symbol functions[] = {
    TAPAS_NATIVE_FUNCTION_DETAIL(
        "int_sum", c_int_sum, 0, UNDEF_NPARAMS,
        "Function[...] -> Int", "int_sum(...values: Int) -> Int")
};
```

The third and fourth count arguments are the minimum and maximum.
`UNDEF_NPARAMS` means that no upper bound exists. The runtime and front end
consume the same descriptor. `tlib_add_cfn` and `tlib_add_cppf` remain as
compatibility interfaces.

Here is a C implementation of an integer sum function:

```c
#include "tapas/tapas.h"

static void c_int_sum(tobj *params, uint_regs len, tobj *vre)
{
    long sum = 0;

    for (uint_regs i = 0; i < len; i++) {
        if (tobj_get_type(&params[i]) != tint) {
            tobj_set_nil(vre);
            return;
        }
        sum += tobj_get_v_tint(&params[i]);
    }

    tobj_set_int(vre, sum);
}
```

Before running Tapas code, place the method table in a root module and install
the extension once:

```c
#include "tapas/tapas.h"

static void c_int_sum(tobj *params, uint_regs len, tobj *vre)
{
    long sum = 0;

    for (uint_regs i = 0; i < len; i++) {
        if (tobj_get_type(&params[i]) != tint) {
            tobj_set_nil(vre);
            return;
        }
        sum += tobj_get_v_tint(&params[i]);
    }

    tobj_set_int(vre, sum);
}

int main(void)
{
    tsession *sess = tsession_new();

    static const textension_symbol functions[] = {
        TAPAS_NATIVE_FUNCTION_DETAIL(
            "int_sum", c_int_sum, 0, UNDEF_NPARAMS,
            "Function[...] -> Int", "int_sum(...values: Int) -> Int")
    };
    static const textension_module root = TAPAS_ROOT_MODULE(functions);
    static const textension_module *const modules[] = { &root };
    static const textension_descriptor extension =
        TAPAS_EXTENSION("example", "1.0", modules);

    tlib_install_extension(tsession_get_lib(sess), &extension);
    tsession_execute_file(sess, "test_extension.tap", 1);

    tsession_free(sess);
    return 0;
}
```

The Tapas file can call `int_sum` like any other function. This fragment is
marked `text` because it requires the preceding custom C host; the standard
Tapas binary does not register `int_sum`:

```text
var s = int_sum(1, 2, 3, 4, 5)
print(s)
```

The output should be 15.

For a package function, replace the root module with a named package:

```c
static const textension_module statistics =
    TAPAS_PACKAGE_MODULE("statistics", functions);
```

### Unified Extension Descriptors

Native standard-library functions and user extensions share one public model,
declared in `include/tapas/textension.h`:

```text
textension_descriptor
└── textension_module[]
    └── textension_symbol[]
```

A symbol is defined once in its descriptor. The runtime materializes functions
and values from it, the compiler reads the same Tapas Type, and the Language
Server uses the same names, signatures, and details for completion and hover.
An extension does not maintain separate runtime, front-end, and editor tables.

`textension_descriptor` stores the ABI version, structure size, extension name,
extension version, and module array. A `textension_module` selects either the
root namespace or a named package through `scope`. Several modules may
contribute distinct names to one package, allowing a large package to be split
across C files by implementation responsibility.

`textension_symbol` describes functions, values, and Types. Its main fields are:

- `name`: the Tapas-visible name;
- `type`: the Tapas Type parsed by the front end and the only static Type source;
- `detail`: the complete human-readable declaration, such as
  `sample::add(left: Int, right: Int) -> Int`;
- `kind`: `textension_function`, `textension_value`, or `textension_type`;
- `function`, `session_function`, and `value_factory`: the one entry matching
  `kind`;
- `minimum_arguments` and `maximum_arguments`: the runtime argument range;
- `intrinsic`: one of the few semantics intrinsic to standard Type construction;
- `result_relation` and `result_argument`: an optional dependent-result
  relation. `tnative_result_declared` uses the result in `type`, while
  `tnative_result_argument` preserves the static Type of one argument.

The argument range remains separate because the current Type syntax cannot
fully express optional and variadic arguments. Ordinary extensions must use
`tnative_intrinsic_none`; they cannot impersonate a standard Type constructor.
A dependent result suits Type-preserving functions such as
`copy(value: T) -> T`. A tunnel call treats its receiver as argument zero, so
one descriptor covers both `copy(value)` and `value.copy()` without separate
front-end or LSP special cases.

Ordinary functions use `tnative_function`. Session functions that need the
current execution environment use `tnative_session_function`. A function symbol
must set exactly one of these entries. Values and Types are materialized at
extension installation through `tnative_value_factory`.

Macros are convenient for short user method tables. A designated initializer
can expose every field directly when readability is more important:

```c
static const textension_symbol functions[] = {
    {
        .name = "add",
        .type = "Function[Int, Int] -> Int",
        .detail = "sample::add(left: Int, right: Int) -> Int",
        .kind = textension_function,
        .function = add,
        .minimum_arguments = 2,
        .maximum_arguments = 2
    }
};

static const textension_module sample = {
    .scope = textension_package,
    .name = "sample",
    .detail = "Sample functions",
    .symbols = functions,
    .symbol_count = sizeof(functions) / sizeof(functions[0])
};

static const textension_module *const modules[] = { &sample };

static const textension_descriptor extension =
    TAPAS_EXTENSION("sample", "1.0", modules);
```

The standard library uses full initializers so that each name, Type, detail,
and call entry remains directly readable. Highly repetitive mathematical
wrappers may use a local macro within one `.c` file, but no cross-file `.def`
protocol is used.

### Validation, Installation, and Ownership

`textension_validate` checks the ABI, structure size, module scope, symbol
entry, argument range, and name conflicts. `tlib_install_extension` installs
only when the complete descriptor is accepted by the current library.
Qualified names cannot be duplicated, root symbols cannot collide with package
names, and existing objects are never replaced implicitly.

The extension owns its descriptors, names, and Type strings. They must remain
unchanged until every Session using them has been destroyed. Consumers may
build read-only indexes but cannot modify the descriptor. The current public
mechanism validates and installs statically linked extensions. Dynamic-library
discovery, handle lifetime, and safe unloading are not implemented. A future
loader should only obtain the common entry point, validate the ABI, and retain
the handle; it should not introduce another module-definition model.
The proposed dynamic-library entry still returns the same descriptor:

```c
const textension_descriptor *tapas_extension(void);
```

Extensions support ordered enumeration and package/name lookup:

```c
uint32_t textension_symbol_count(const textension_descriptor *extension);
int textension_symbol_at(const textension_descriptor *extension,
                         uint32_t index,
                         textension_symbol_ref *result);
int textension_find(const textension_descriptor *extension,
                    const char *package,
                    const char *name,
                    textension_symbol_ref *result);
```

### How the Standard Library Uses the Model

`include/tapas/tstdlib.h` exposes only the standard extension descriptor:

```c
const textension_descriptor *tstdlib_descriptor(void);
```

`src/stdlib/tstdlib.c` collects modules, while `src/stdlib/modules.h` only
declares them and does not hold another signature table. The current layout is
organized by Tapas-visible responsibility:

```text
src/stdlib/
  tstdlib.c
  modules.h
  arguments.h
  builtins/
    console.c
    conversion.c
    list.c
    array.c
    pair.c
    capability.c
    iterator.c
    dict.c
    sort.c
    objects.c
    time.c
    session.c
    rules.c
  dense/dense.c
  evaluators/evaluators.c
  io/io.c
  math/math.c
  rules/rules.c
  syntax/syntax.c
  time/time.c
  types/types.c
  format/
    __init__.tap
    format.tap
```

Modules under `builtins/` export root functions. Native modules under named
directories export the corresponding Tapas packages. `format` is a source
standard package implemented in ordinary Tapas and is not part of the native
extension descriptor. Native and source packages form one standard library,
but only facilities requiring the C runtime or a host boundary use
`textension_descriptor`.

A native object's lifetime, operators, and generic capabilities belong to its
`tcompo_vtable` and `tcompo_capabilities`, rather than being repeated on every
constructor symbol. Extension descriptors reuse the Tapas Type language.
The few standard Type constructors that cannot be described declaratively use
`intrinsic`; ordinary functions cannot attach private Type checkers.




## Working With Tapas Values

Tapas values are represented by the tagged union type `tobj`.

The main runtime type codes are:

```c
tnil
tbool
tint
tfloat
tcompo
```

Use the helper functions in `tapas/tval.h` rather than modifying fields
directly:

```c
void tobj_set_nil(tobj *v);
void tobj_set_bool(tobj *v, int b);
void tobj_set_int(tobj *v, long i);
void tobj_set_float(tobj *v, double d);
void tobj_set_compo(tobj *v, tcompo_v *compo);

ttypes tobj_get_type(const tobj *v);
long tobj_get_v_tint(const tobj *v);
double tobj_get_v_tfloat(const tobj *v);
int tobj_get_v_tbool(const tobj *v);
tcompo_v *tobj_get_v_tcompo(const tobj *v);
```

Composite Tapas values, such as strings, lists, dictionaries, arrays,
functions, libraries, and time values, are reference values. When returning or
storing composite values, use the existing constructor and setter functions so
that reference counts remain consistent.

`tapas/tval.h` declares only the core value system. Include the corresponding
header when using a concrete runtime type:

```text
#include "tapas/runtime/tstr.h"
#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tpair.h"
#include "tapas/runtime/tdict.h"
#include "tapas/runtime/titer.h"
#include "tapas/runtime/tarray.h"
#include "tapas/runtime/ttime.h"
#include "tapas/runtime/tcfn.h"
```

The built-in collection constructors include:

```c
tstr *tstr_new(const char *s);
tlist *tlist_new(void);
tpair *tpair_new(const tobj *first, const tobj *second);
tdict *tdict_new(void);
titer *titer_new(long start, long end);

tdarr *tdarr_new(size_t rows, size_t cols, double value);
tbarr *tbarr_new(size_t rows, size_t cols, int value);
ttime *ttime_new(void);
ttime *ttime_from_time(time_t value);
time_t ttime_get(const ttime *value);
```

Unix conversion and arbitrary formatting belong to the stdlib `time` package;
the runtime API only exposes the representation boundary through `time_t`.

Lists and dictionaries retain composite values inserted into them. Dense arrays
own contiguous row-major storage and expose checked accessors:

```c
double tdarr_at(const tdarr *arr, size_t row, size_t col);
void tdarr_set(tdarr *arr, size_t row, size_t col, double value);
int tbarr_at(const tbarr *arr, size_t row, size_t col);
void tbarr_set(tbarr *arr, size_t row, size_t col, int value);

tdarr *tdarr_matmul(const tdarr *left, const tdarr *right);
```

Transpose is a `dense` standard-library operation rather than a runtime object
primitive. RealArray negation is dispatched through `tcompo_vtable.op_neg`;
`tdarr_matmul` implements matrix multiplication and loads
a compatible LP64 CBLAS dynamic library at runtime. Tapas itself does not link
BLAS while building or installing. Set
`TAPAS_BLAS_LIBRARY` to select the library explicitly. Matrix multiplication reports
a Tapas runtime error when no compatible backend can be loaded.

Use `tobj_set_compo` when returning a newly constructed composite value from a
C function:

```c
static void make_identity(tobj *params, uint_regs len, tobj *vre)
{
    (void)params;
    if (len != 0) {
        tobj_set_nil(vre);
        return;
    }

    tdarr *matrix = tdarr_new(2, 2, 0.0);
    tdarr_set(matrix, 0, 0, 1.0);
    tdarr_set(matrix, 1, 1, 1.0);
    tobj_set_compo(vre, (tcompo_v *)matrix);
}
```


## Extending Tapas With C Data Types

Custom C data types can be exposed to Tapas by embedding `tcompo_v` as the first
field of the C struct and providing a `tcompo_vtable`.

At minimum, a composite value must provide functions for:

- Returning its Tapas type name.
- Returning its composite type code.
- Reporting its length.
- Copying itself.
- Freeing itself.
- Testing identity.
- Formatting itself as a string.

The core vtable type also contains optional binary operator hooks:

```c
typedef struct {
    compo_get_type_fn get_type;
    compo_get_code_fn get_compo_type_code;
    compo_len_fn len;
    compo_copy_fn copy;
    compo_free_fn free;
    compo_identical_fn identical;
    compo_tostring_abbr_fn tostring_abbr;
    compo_tostring_full_fn tostring_full;

    compo_op_bin_fn op_add;
    compo_op_bin_fn op_sub;
    compo_op_bin_fn op_mul;
    compo_op_bin_fn op_div;
    compo_op_bin_fn op_mod;
    compo_op_bin_fn op_pow;
    compo_op_bin_fn op_mmul;
    compo_op_bin_fn op_eq;
    compo_op_bin_fn op_ne;
    compo_op_bin_fn op_sg;
    compo_op_bin_fn op_sl;
    compo_op_bin_fn op_ge;
    compo_op_bin_fn op_le;
    compo_op_bin_fn op_and;
    compo_op_bin_fn op_or;
    const tcompo_capabilities *capabilities;
} tcompo_vtable;
```

Optional object protocols are grouped separately from operators:

```c
typedef struct {
    tcompo_index_fn indexable;
    tcompo_index_set_fn index_settable;
    tcompo_append_fn appendable;
    tcompo_delete_fn deletable;
    tcompo_contains_fn contains;
    tcompo_next_fn iterable;
} tcompo_capabilities;
```

Leave unsupported entries null. Index reads, index writes, `append`, `delete`,
`in`, and `for` dispatch through these slots without testing a concrete object
type. An iterable callback receives caller-owned cursor storage; it increments
the position and returns 1 when it produces a value, and returns 0 when
exhausted. It must not keep a shared cursor in the object itself.

After creating the C type, expose a C function that constructs an instance and
returns it with `tobj_set_compo`, then install the constructor as a
`textension_symbol`. Tapas code can create custom values and the front end reads
the call signature from the same extension descriptor. `tcfn_descriptor` and
`tlib_add_cfn` remain compatibility APIs for old direct-registration code; new
extensions should not use them to create a separate signature source.
