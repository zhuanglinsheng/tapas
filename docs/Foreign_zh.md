# Tapas 与 C 交互

简体中文 | [English](Foreign_en.md)

本文说明如何从 C 调用 Tapas 代码，以及如何向 Tapas 脚本公开 C 函数。

公开 C API 声明在 `include/tapas` 下的头文件中。大多数使用者只需包含
`tapas/tapas.h`；该文件包含会话 API 和运行时值类型。



## 从 C 调用 Tapas 脚本

Tapas 源文件是后缀为 `.tap` 的文本文件。C 程序有两种运行方式：

- 将 `.tap` 编译为 `.tapc` 字节码，再执行字节码。
- 直接执行 `.tap`，不保留 `.tapc` 文件。

主要会话函数如下：

```c
tsession *tsession_new(void);
void tsession_free(tsession *sess);

void tsession_compile_file(tsession *sess, const char *file, int interactive);
void tsession_eval_bycodes(tsession *sess, const char *file);
void tsession_execute_file(tsession *sess, const char *file, int interactive);
void tsession_execute_str(tsession *sess, const char *str, int interactive);
```

例如，假设 `test_calling.tap` 包含：

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

下面的 C 程序编译并执行该文件：

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

不需要保存字节码时，改用 `tsession_execute_file`：

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

也可以直接执行 C 字符串中的 Tapas 代码：

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



## 使用 C 函数扩展 Tapas

把 C 函数包装为 Tapas 可调用值并加入当前会话库，即可向 Tapas 公开它们。

函数指针类型为：

```c
typedef void (*genf_t)(tobj *params, uint_regs len, tobj *vre);
```

参数含义如下：

- `params`：脚本传入的 Tapas 值数组。
- `len`：参数数量。
- `vre`：接收函数结果的输出值。

使用以下函数进行注册：

```c
void tlib_add_cppf(tlib *lb, const char *name, genf_t f, uint_regs nparams_sig);
```

最后一个参数 `nparams_sig` 是期望的参数数量。函数接受可变参数时使用
`UNDEF_NPARAMS`。

下面用 C 实现一个整数求和函数：

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

运行 Tapas 脚本前注册它：

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

Tapas 文件可以像调用普通函数一样调用 `int_sum`。下面的示例依赖前面的自定义 C
宿主，而标准 Tapas 二进制没有注册 `int_sum`，因此这里标记为 `text`：

```text
var s = int_sum(1, 2, 3, 4, 5)
print(s)
```

输出应为 15。




## 操作 Tapas 值

Tapas 值由带标签联合类型 `tobj` 表示。

主要运行时类型码如下：

```c
tnil
tbool
tint
tfloat
tcompo
```

应使用 `tapas/tval.h` 中的辅助函数，不要直接修改字段：

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

字符串、列表、字典、数组、函数、库和时间等复合 Tapas 值都是引用值。返回或
保存复合值时，应使用现有构造器和设置函数，以保持引用计数一致。

`tapas/tval.h` 只声明核心值系统。使用具体运行时类型时，应包含对应的独立
头文件：

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

内置集合构造器包括：

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

`ttime_from_unix`、`ttime_unix` 和 `ttime_shift` 使用整数秒；
`ttime_format` 使用本地时区，并接受宿主 C `strftime` 的格式字符串。

列表和字典会保留插入其中的复合值。稠密数组使用连续的行主序内存，并提供会
检查下标的访问函数：

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

`tdarr_neg` 和 `tdarr_matmul` 需要在运行时加载兼容的 LP64 CBLAS 动态库；
Tapas 本身不会在编译或安装时链接 BLAS。可以通过 `TAPAS_BLAS_LIBRARY` 指定
动态库。找不到兼容后端时，这两个函数会报告 Tapas 运行时错误。

C 函数返回新构造的复合值时应使用 `tobj_set_compo`：

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



## 使用 C 数据类型扩展 Tapas

要向 Tapas 公开自定义 C 数据类型，应把 `tcompo_v` 嵌入为 C 结构体的第一个
字段，并提供一个 `tcompo_vtable`。

复合值至少必须提供以下函数：

- 返回 Tapas 类型名；
- 返回复合类型码；
- 报告长度；
- 复制自身；
- 释放自身；
- 检查同一性；
- 格式化为字符串。

核心虚函数表还包含可选的二元运算钩子：

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

创建 C 类型后，应公开一个构造实例并通过 `tobj_set_compo` 返回它的 C 函数。
用 `tlib_add_cppf` 注册该构造器后，Tapas 代码即可创建自定义类型的值。
