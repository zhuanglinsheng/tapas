# Tapas 与 C 交互

简体中文 | [English](Foreign_en.md) | [项目主页](../README.md)

本文说明如何从 C 调用 Tapas 代码，以及如何向 Tapas 脚本公开 C 函数。

公开 C API 声明在`include/tapas`下的头文件中。
大多数使用者只需包含`tapas/tapas.h`；该文件包含会话 API 和运行时值类型。

## 从 C 调用 Tapas 脚本

Tapas 源文件是后缀为`.tap`的文本文件。
C 程序有两种运行方式：

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

例如，假设`test_calling.tap`包含：

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

不需要保存字节码时，改用`tsession_execute_file`：

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

推荐使用原生扩展描述符。
函数实现、名称、Tapas Type 和参数范围定义在同一处：

```c
static const textension_symbol functions[] = {
    TAPAS_NATIVE_FUNCTION_DETAIL(
        "int_sum", c_int_sum, 0, UNDEF_NPARAMS,
        "Function[...] -> Int", "int_sum(...values: Int) -> Int")
};
```

`TAPAS_NATIVE_FUNCTION`的第三、第四个参数分别是最少和最多参数数量。
`UNDEF_NPARAMS`表示没有上限。
运行时和前端读取同一描述符。
旧的`tlib_add_cfn`与`tlib_add_cppf`仍作为兼容接口保留。

下面用 C 实现一个整数求和函数：

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

运行 Tapas 脚本前，将方法表放入根模块和扩展描述符并一次安装：

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

Tapas 文件可以像调用普通函数一样调用`int_sum`。
下面的示例依赖前面的自定义 C 宿主，而标准 Tapas 二进制没有注册`int_sum`，因此这里标记为`text`：

```text
var s = int_sum(1, 2, 3, 4, 5)
print(s)
```

输出应为 15。

包函数只需把根模块换成命名包：

```c
static const textension_module statistics =
    TAPAS_PACKAGE_MODULE("statistics", functions);
```

### 统一扩展描述符

标准库原生函数和用户原生扩展使用同一个公共模型，定义在`include/tapas/textension.h`：

```text
textension_descriptor
└── textension_module[]
    └── textension_symbol[]
```

一个符号只在描述符中定义一次。
运行时据此创建函数和值，编译器读取同一份 Type，Language Server 使用同一份名称、签名和详情生成补全与悬停信息。
扩展不需要分别维护运行时注册表、前端签名表和编辑器清单。

`textension_descriptor`保存 ABI 版本、结构大小、扩展名称、扩展版本和 module 数组。
`textension_module`的`scope`明确选择根命名空间或命名包。
多个 module 可以向同一个包贡献不同名称，因此一个较大的包可以按实现职责拆成多个 C 文件。

`textension_symbol`统一描述函数、值和 Type，主要字段为：

- `name`：Tapas 中可见的名称；
- `type`：前端解析的 Tapas Type，是静态类型信息的唯一来源；
- `detail`：面向人的完整声明，例如`sample::add(left: Int, right: Int) -> Int`；
- `kind`：`textension_function`、`textension_value`或`textension_type`；
- `function`、`session_function`、`value_factory`：与 `kind` 对应的唯一入口；
- `minimum_arguments`、`maximum_arguments`：运行时参数范围；
- `intrinsic`：标准类型构造等少数前端固有语义；
- `result_relation`、`result_argument`：可选的依赖结果关系。默认的`tnative_result_declared`直接采用`type`中的结果；`tnative_result_argument`表示结果保留指定实参的静态 Type。

参数范围独立存在，是因为可选参数和变长参数还不能完全由当前 Type 语法表达。
普通扩展的`intrinsic`必须使用`tnative_intrinsic_none`；它不能冒充标准类型构造。
依赖结果关系适合`copy(value: T) -> T`这类类型保持函数。
管道调用把接收者视为第 0 个实参，因此同一描述同时覆盖`copy(value)`和`value.copy()`，前端与 LSP 不需要各自维护函数特例。

普通函数使用`tnative_function`，需要访问当前执行环境的会话函数使用`tnative_session_function`。
一个函数符号必须且只能设置其中一个入口。
值和 Type 通过`tnative_value_factory`创建，每次安装扩展时由运行时物化。

宏适合用户扩展的简短方法表；需要完整展示元数据时，也可以直接使用 designated initializer：

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

标准库使用完整 initializer，使每个符号的名称、类型、详情和调用入口都能直接阅读；高度重复的数学包装器可以在单个`.c`文件内使用局部宏，但不使用跨文件`.def`协议。

### 校验、安装和所有权

`textension_validate`检查 ABI、结构大小、module 范围、符号入口、参数范围和名称冲突。
`tlib_install_extension`只有在整个描述符能被当前库接受时才安装：相同限定名不能重复，根符号不能和包名冲突，也不会隐式覆盖已有对象。

描述符、名称和 Type 字符串由扩展拥有，在所有使用它们的 Session 销毁前必须保持不变。
消费者可以建立只读索引，但不能修改描述符。
当前公开机制负责静态链接扩展的校验与安装；动态库发现、句柄生命周期和安全卸载尚未实现。
未来的动态加载器应只负责取得统一入口、检查 ABI 并保持句柄存活，不应引入第二种 module 定义方式。
建议的动态库入口仍返回同一种描述符：

```c
const textension_descriptor *tapas_extension(void);
```

扩展可以按顺序枚举，也可以按包名和符号名查询：

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

### 标准库如何使用扩展模型

`include/tapas/tstdlib.h`只公开标准扩展描述符：

```c
const textension_descriptor *tstdlib_descriptor(void);
```

`src/stdlib/tstdlib.c`汇总 module；`src/stdlib/modules.h`只声明各 module，不保存另一份签名。
当前布局按语言可见职责组织：

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

`builtins/`中的 module 导出根函数；命名目录中的原生 module 导出相应 Tapas 包。
`format`是普通 Tapas 实现的源码标准包，不进入原生扩展描述符。
原生包和源码包共同组成标准库，但只有需要 C 运行时或宿主边界的部分使用`textension_descriptor`。

原生对象的生命周期、运算符和通用能力属于对象的`tcompo_vtable`与`tcompo_capabilities`，不在每个构造函数符号中重复。
扩展描述符复用 Tapas 自身的 Type 语言；无法声明式表达的少量标准类型构造使用`intrinsic`，不能为普通函数附加任意的私有类型检查器。

## 操作 Tapas 值

Tapas 值由带标签联合类型`tobj`表示。

主要运行时类型码如下：

```c
tnil
tbool
tint
tfloat
tcompo
```

应使用`tapas/tval.h`中的辅助函数，不要直接修改字段：

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

字符串、列表、字典、数组、函数、库和时间等复合 Tapas 值都是引用值。
返回或保存复合值时，应使用现有构造器和设置函数，以保持引用计数一致。

`tapas/tval.h`只声明核心值系统。
使用具体运行时类型时，应包含对应的独立头文件：

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
ttime *ttime_from_time(time_t value);
time_t ttime_get(const ttime *value);
```

Unix 转换和任意格式化属于标准库`time`包；runtime API 只通过`time_t`暴露表示层边界。

列表和字典会保留插入其中的复合值。
稠密数组使用连续的行主序内存，并提供会检查下标的访问函数：

```c
double tdarr_at(const tdarr *arr, size_t row, size_t col);
void tdarr_set(tdarr *arr, size_t row, size_t col, double value);
int tbarr_at(const tbarr *arr, size_t row, size_t col);
void tbarr_set(tbarr *arr, size_t row, size_t col, int value);

tdarr *tdarr_matmul(const tdarr *left, const tdarr *right);
```

转置属于`dense`标准库操作，不是 runtime 对象原语。
RealArray 负号通过`tcompo_vtable.op_neg`分派；`tdarr_matmul`实现矩阵乘法，需要在运行时加载兼容的 LP64 CBLAS 动态库；Tapas 本身不会在编译或安装时链接 BLAS。
可以通过`TAPAS_BLAS_LIBRARY`指定动态库。
找不到兼容后端时，矩阵乘法会报告 Tapas 运行时错误。

C 函数返回新构造的复合值时应使用`tobj_set_compo`：

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

## 使用 C 数据类型扩展 Tapas

要向 Tapas 公开自定义 C 数据类型，应把`tcompo_v`嵌入为 C 结构体的第一个字段，并提供一个`tcompo_vtable`。

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
    const tcompo_capabilities *capabilities;
} tcompo_vtable;
```

对象的可选协议与运算符分开集中声明：

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

对象不支持的槽保持为空。
索引读取、索引写入、`append`、`delete`、`in`和`for`都通过这些槽分派，不检查对象的具体类型。
`iterable`回调使用调用者持有的游标；产出值时递增位置并返回 1，耗尽时返回 0，不能把共享游标保存在对象内。

创建 C 类型后，应公开一个构造实例并通过`tobj_set_compo`返回它的 C 函数，再把构造器作为`textension_symbol`安装。
这样 Tapas 代码可以创建自定义值，前端也从同一个扩展描述符读取调用签名。
`tcfn_descriptor`与`tlib_add_cfn`只用于兼容旧的直接注册代码，新扩展不应再用它们建立独立签名来源。
