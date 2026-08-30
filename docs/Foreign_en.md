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
var abs = (x){
    if(x >= 0){
        return x
    }
    else{
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

Register the function with:

```c
void tlib_add_cppf(tlib *lb, const char *name, genf_t f, uint_regs nparams_sig);
```

The last argument, `nparams_sig`, is the expected parameter count. Use
`UNDEF_NPARAMS` when the function accepts a variable number of arguments.

Here is a C implementation of an integer sum function:

```c
#include "tapas/tapas.h"

static void c_int_sum(tobj *params, uint_regs len, tobj *vre)
{
    long sum = 0;

    for(uint_regs i = 0; i < len; i++){
        if(tobj_get_type(&params[i]) != tint){
            tobj_set_nil(vre);
            return;
        }
        sum += tobj_get_v_tint(&params[i]);
    }

    tobj_set_int(vre, sum);
}
```

Then register it before running the Tapas script:

```c
#include "tapas/tapas.h"

static void c_int_sum(tobj *params, uint_regs len, tobj *vre)
{
    long sum = 0;

    for(uint_regs i = 0; i < len; i++){
        if(tobj_get_type(&params[i]) != tint){
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

    tlib_add_cppf(tsession_get_lib(sess), "int_sum", c_int_sum, UNDEF_NPARAMS);
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
ttime *ttime_from_unix(long seconds);
long ttime_unix(const ttime *value);
ttime *ttime_shift(const ttime *value, long seconds);
tstring *ttime_format(const ttime *value, const char *pattern);
```

`ttime_from_unix`, `ttime_unix`, and `ttime_shift` use whole seconds.
`ttime_format` uses local time and accepts a host C `strftime` pattern.

Lists and dictionaries retain composite values inserted into them. Dense arrays
own contiguous row-major storage and expose checked accessors:

```c
double tdarr_at(const tdarr *arr, size_t row, size_t col);
void tdarr_set(tdarr *arr, size_t row, size_t col, double value);
int tbarr_at(const tbarr *arr, size_t row, size_t col);
void tbarr_set(tbarr *arr, size_t row, size_t col, int value);

tdarr *tdarr_transpose(const tdarr *arr);
tbarr *tbarr_transpose(const tbarr *arr);
tdarr *tdarr_neg(const tdarr *arr);
tdarr *tdarr_matmul(const tdarr *left, const tdarr *right);
```

`tdarr_neg` and `tdarr_matmul` load a compatible LP64 CBLAS dynamic library at
runtime. Tapas itself does not link BLAS while building or installing. Set
`TAPAS_BLAS_LIBRARY` to select the library explicitly. These functions report
a Tapas runtime error when no compatible backend can be loaded.

Use `tobj_set_compo` when returning a newly constructed composite value from a
C function:

```c
static void make_identity(tobj *params, uint_regs len, tobj *vre)
{
    (void)params;
    if(len != 0){
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
} tcompo_vtable;
```

After creating the C type, expose a C function that constructs an instance and
returns it with `tobj_set_compo`. Register that constructor with
`tlib_add_cppf`, and Tapas code can create values of the custom type.
