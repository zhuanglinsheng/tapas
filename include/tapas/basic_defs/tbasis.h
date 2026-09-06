/**
 * @file tbasis.h
 * @brief 声明 Tapas 各层共享的基础整数类型、协议上限与错误接口。
 *
 * @details 这是 Tapas 最底层的公共头文件，只包含语言核心各层共同依赖的标量
 * 别名、字节码基础编号、诊断信息和配置常量。
 *
 * @note 不得利用本文件的传递包含散布无关的 C 标准库声明或 Tapas 上层接口；新增
 * 内容必须确实属于跨层共享的基础协议。
 */
#ifndef TAPAS_TBASIS_H
#define TAPAS_TBASIS_H

#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
#error "Tapas requires an ISO C23 compiler"
#endif

#include <setjmp.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tstring tstring; /**< 动态字符串；完整定义位于 `dsa/tstring.h`。 */


/* 优化开关 */

/** 启用静态阶段优化。 */
#define Tap_Static_OPT

/** 启用运行时优化。 */
#define Tap_Runtime_OPT


/* 数值范围 */

/** Tapas 整数允许的最大值。 */
#define Tap_Long_MAX (2e31 - 1)


/* 基础整数别名 */

typedef uint64_t uint_lexs; /**< 词法位置使用的无符号整数。 */
typedef int64_t int_lexs;   /**< 词法位置差值使用的有符号整数。 */

typedef uint32_t uint_cmds; /**< 字节码指令位置与数量。 */

typedef uint32_t uint_csts; /**< 常量池位置与数量。 */

typedef uint16_t uint_objs; /**< 环境对象位置与数量。 */

typedef uint8_t uint_regs;  /**< 寄存器位置与参数数量。 */


/* 编码上限 */

/** U 操作数可表示的最大值。 */
#define Limit_U ((uint32_t)67108863)
/** 常量池索引可表示的最大值。 */
#define Limit_C ((uint32_t)262143)
/** L 或对象槽位可表示的最大值。 */
#define Limit_L ((uint16_t)8191)
/** 参数或寄存器索引可表示的最大值。 */
#define Limit_P ((uint8_t)255)

/** 字节码指令数量上限。 */
#define CMD_LIMIT Limit_U
/** 常量数量上限。 */
#define CST_LIMIT Limit_C
/** 对象槽位数量上限。 */
#define OBJ_LIMIT Limit_L
/** 寄存器数量上限。 */
#define REG_LIMIT Limit_P


/* 特殊标记 */

/** 表示未指定参数数量的哨兵值。 */
#define UNDEF_NPARAMS REG_LIMIT
/** 表示未关联名称槽位的哨兵值。 */
#define UNDEF_NAMELOC CST_LIMIT
/** 表示未关联环境槽位的哨兵值。 */
#define UNDEF_ENVLOC OBJ_LIMIT

/* OP_PUSHX 地址编码 */

/* OP_PUSHX 的 L 保存槽位，R 按以下方式编码：
 *   第 0 位：isenv（0 表示临时值，1 表示环境值）
 *   第 1 位：is_upval（0 表示当前环境，1 表示外层环境）
 *   第 2 至 12 位：depth（沿 father_env 向外查找的层数）
 *
 * 常用 R 值：
 *   0  -> tmp[L]
 *   1  -> 当前环境的 L 槽位
 *   7  -> 向外一层环境的 L 槽位
 *   11 -> 向外两层环境的 L 槽位
 */

/**
 * @brief 返回临时值槽位使用的 R 编码。
 *
 * @return 临时值地址编码。
 */
static inline uint16_t tpushx_tmp_addr(void)
{
	return 0;
}

/**
 * @brief 返回当前环境槽位使用的 R 编码。
 *
 * @return 当前环境地址编码。
 */
static inline uint16_t tpushx_local_addr(void)
{
	return 1;
}

/**
 * @brief 根据向外查找深度构造闭包捕获槽位的 R 编码。
 *
 * @param depth 沿外层环境向上查找的层数。
 * @return 外层环境地址编码。
 */
static inline uint16_t tpushx_upval_addr(uint16_t depth)
{
	return (uint16_t)(1u | 2u | ((uint16_t)depth << 2));
}

/**
 * @brief 判断 R 编码是否指向环境槽位。
 *
 * @param r R 地址编码。
 * @return 指向环境槽位返回非零，否则返回零。
 */
static inline int tpushx_isenv(uint16_t r)
{
	return (int)(r & 1u);
}

/**
 * @brief 判断 R 编码是否指向外层环境。
 *
 * @param r R 地址编码。
 * @return 指向外层环境返回非零，否则返回零。
 */
static inline int tpushx_is_upval(uint16_t r)
{
	return (int)((r >> 1) & 1u);
}

/**
 * @brief 从 R 编码中读取向外查找深度。
 *
 * @param r R 地址编码。
 * @return 向外查找的环境层数。
 */
static inline uint16_t tpushx_depth(uint16_t r)
{
	return (uint16_t)(r >> 2);
}


/* 字节码指令 */

/** 编译器与 VM 共同使用的操作码。 */
typedef enum {
	OP_PASS,       /**< 空操作。 */
	OP_VCRT,       /**< 创建变量槽位。 */
	OP_TMPDEL,     /**< 释放临时值。 */
	OP_THIS,       /**< 压入当前对象。 */
	OP_BASE,       /**< 压入基础对象。 */
	OP_RET,        /**< 从当前函数返回。 */
	OP_IN,         /**< 执行成员判断。 */
	OP_PAIR,       /**< 构造 Pair。 */
	OP_TO,         /**< 执行转换。 */
	OP_POPN,       /**< 弹出多个栈值。 */
	OP_POPCOV,     /**< 弹出闭包覆盖值。 */
	OP_TYPEFWD,    /**< 声明递归 Type 占位符。 */
	OP_TYPEDEFINE, /**< 定义递归 Type 主体。 */
	OP_LOOPAS,     /**< 绑定循环迭代值。 */
	OP_JPF,        /**< 向前跳转。 */
	OP_JPB,        /**< 向后跳转。 */
	OP_CJPFPOP,    /**< 条件向前跳转并弹栈。 */
	OP_CJPBPOP,    /**< 条件向后跳转并弹栈。 */
	OP_PUSHX,      /**< 压入槽位或闭包值。 */
	OP_PUSHI,      /**< 压入整数常量。 */
	OP_PUSHFLT,    /**< 压入浮点常量。 */
	OP_PUSHB,      /**< 压入布尔常量。 */
	OP_PUSHS,      /**< 压入字符串常量。 */
	OP_PUSHDICT,   /**< 创建并压入 Dictionary。 */
	OP_PUSHINFO,   /**< 压入编译信息。 */
	OP_IMPORT,     /**< 导入模块或符号。 */
	OP_IDXR,       /**< 执行索引读取。 */
	OP_EVAL,       /**< 调用普通可调用值。 */
	OP_EVALSF,     /**< 调用会话原生函数。 */
	OP_EVALCF,     /**< 调用普通原生函数。 */
	OP_EVALTF,     /**< 调用 Tapas 函数。 */
	OP_IDXL,       /**< 执行索引写入。 */
	OP_PUSHF,      /**< 创建并压入函数。 */
	OP_PUSHRULE,   /**< 创建并压入 Rule。 */
	OP_RULECOND,   /**< 记录 Rule 条件。 */
	OP_RULEREQ,    /**< 记录 Rule 要求。 */
	OP_ADD,        /**< 加法。 */
	OP_SUB,        /**< 减法。 */
	OP_MUL,        /**< 乘法。 */
	OP_DIV,        /**< 除法。 */
	OP_MOD,        /**< 取模。 */
	OP_POW,        /**< 幂运算。 */
	OP_MMUL,       /**< 矩阵乘法。 */
	OP_EQ,         /**< 相等比较。 */
	OP_NE,         /**< 不等比较。 */
	OP_GE,         /**< 大于等于比较。 */
	OP_SG,         /**< 大于比较。 */
	OP_LE,         /**< 小于等于比较。 */
	OP_SL,         /**< 小于比较。 */
	OP_AND,        /**< 逻辑与。 */
	OP_OR,         /**< 逻辑或。 */
	OP_POS,        /**< 一元正号。 */
	OP_NEG,        /**< 一元负号。 */
	OP_BAND,       /**< 短路逻辑与。 */
	OP_BOR,        /**< 短路逻辑或。 */
	OP_FUNCMETA,   /**< 关联函数元数据。 */
	OP_BINDTYPE,   /**< 绑定 Type。 */
	OP_CHECKTYPE,  /**< 检查值的 Type。 */
	OP_RULETYPE,   /**< 构造 Rule 相关 Type。 */
	OP_RULENOT,    /**< 构造 Rule 逻辑非 Term。 */
	OP_RULETRUTH,  /**< 读取 Rule 真值。 */
	OP_RULEITEM,   /**< 构造 RuleItem。 */
	OP_RULEVALUE,  /**< 读取 Rule 求值结果。 */
	OP_COUNT       /**< 操作码数量，不是可执行指令。 */
} tins;


/* 值存储类别 */

/**
 * @brief `tobj` 的顶层存储类别。
 *
 * @details 该编号只区分立即值与引用对象，不等同于语言层的 Type 身份。
 */
typedef enum {
	tnil    = 0, /**< Nil 立即值。 */
	tbool   = 1, /**< Boolean 立即值。 */
	tint    = 2, /**< Integer 立即值。 */
	tfloat  = 3, /**< Float 立即值。 */
	tcompo  = 4  /**< 由引用计数管理的对象。 */
} ttypes;


/* 引用对象运行时表示编号 */

/**
 * @brief 核心引用对象的运行时表示编号。
 *
 * @details VM 使用这些编号选择对象布局和底层操作；它们不是语言层的 Type 身份。
 * 包对象统一使用 `compo_extension`，再通过对象能力提供精确的限定 Type。
 *
 * @note 编号属于持久化与扩展 ABI。空缺编号为兼容旧数据而保留，不得重新使用。
 */
typedef enum {
	compo_tstr     = 0,  /**< String 对象。 */
	compo_tlist    = 1,  /**< List 对象。 */
	compo_tpair    = 2,  /**< Pair 对象。 */
	compo_tdict    = 3,  /**< Dictionary 对象。 */
	compo_tfunc    = 4,  /**< Tapas 函数对象。 */
	compo_tlib     = 5,  /**< 符号库对象。 */
	compo_titer    = 6,  /**< 整数迭代器对象。 */
	compo_tbarr    = 7,  /**< 布尔数组对象。 */
	compo_tdarr    = 8,  /**< 实数数组对象。 */
	compo_tarr     = 9,  /**< 为旧版抽象数组表示保留的编号。 */
	compo_cppfunc  = 10, /**< 普通原生函数对象。 */
	compo_sessfunc = 11, /**< 会话原生函数对象。 */
	compo_time     = 12, /**< Time 对象。 */
	compo_ttypeval = 13, /**< Type 对象。 */
	compo_trule    = 14, /**< Rule 对象。 */
	compo_trule_instance = 15, /**< RuleInstance 对象。 */
	compo_trule_builtin = 16,  /**< 内建 Rule 可调用对象。 */
	/* 编号 17 曾由包对象占用，为保持核心编号稳定而保留。 */
	compo_trule_ir = 18,   /**< RuleIR 对象。 */
	compo_trule_term = 19, /**< RuleTerm 对象。 */
	compo_trule_item = 20, /**< RuleItem 对象。 */
	/* 编号 21 和 22 曾由包对象占用，为保持 ABI 稳定而保留。 */
	/* 包对象共享该编号，并通过能力接口暴露限定 Type。 */
	compo_extension = 23 /**< 包定义的扩展对象。 */
} tcompo_type;


/* 错误类别 */

/** 编译、会话和运行阶段可能报告的错误类别。 */
typedef enum {
	ErrCompile_Other,          /**< 其他编译错误。 */
	ErrCompile_UnfoundFile,    /**< 找不到源文件。 */
	ErrCompile_BracketsOpen,   /**< 括号未闭合。 */
	ErrCompile_VarNoType,      /**< 无法确定变量 Type。 */
	ErrCompile_DblVDeclare,    /**< 变量重复声明。 */
	ErrCompile_InBlkVarDef,    /**< 在不允许的位置定义块内变量。 */
	ErrCompile_ObjUnfound,     /**< 找不到引用的符号。 */
	ErrCompile_InvalidVname,   /**< 变量名称无效。 */
	ErrCompile_InvalidLiter,   /**< 字面量无效。 */
	ErrCompile_AsgDefault,     /**< 默认值赋值无效。 */
	ErrCompile_REGOutOfLimit,  /**< 寄存器数量超过协议上限。 */
	ErrCompile_CMDOutOfLimit,  /**< 指令数量超过协议上限。 */
	ErrCompile_OBJOutOfLimit,  /**< 对象槽位超过协议上限。 */
	ErrCompile_CSTOutOfLimit,  /**< 常量数量超过协议上限。 */
	ErrCompile_ReturnTmpObj,   /**< 试图返回生命周期不足的临时对象。 */
	ErrCompile_InvalidFile,    /**< 输入文件格式无效。 */

	ErrSession_IO,             /**< 会话输入输出失败。 */

	ErrRuntime_Other,          /**< 其他运行时错误。 */
	ErrRuntime_DivIntZero,     /**< 整数除以零。 */
	ErrRuntime_ParamsCtr,      /**< 参数数量不符合要求。 */
	ErrRuntime_ParamsType,     /**< 参数 Type 不符合要求。 */
	ErrRuntime_IdxOutRange,    /**< 索引超出范围。 */
	ErrRuntime_InvalidIndex,   /**< 索引值或索引方式无效。 */
	ErrRuntime_LoopRef,        /**< 循环引用无效。 */
	ErrRuntime_RefType,        /**< 引用值的 Type 无效。 */
	ErrRuntime_LenInconsis,    /**< 操作数长度不一致。 */
	ErrRuntime_AssignNil,      /**< 不允许把 Nil 赋给目标。 */
	ErrRuntime_ObjUnfound,     /**< 运行时找不到对象。 */
	ErrRuntime_IntOutOfRange,  /**< 整数超出允许范围。 */
	ErrRuntime_RefEmptySet,    /**< 引用指向空集合。 */
	ErrRuntime_StringEval,     /**< 字符串求值失败。 */
	ErrRuntime_EnvInconsis,    /**< 执行环境状态不一致。 */
	ErrRuntime_RecurseRefRet   /**< 递归引用返回值无效。 */
} terror_type;

/* 错误上下文接口 */

/** 错误发生的处理阶段。 */
typedef enum {
	TErrorPhase_Compile, /**< 编译阶段。 */
	TErrorPhase_Runtime, /**< 运行阶段。 */
	TErrorPhase_Session, /**< 会话和输入输出阶段。 */
	TErrorPhase_Unknown  /**< 无法确定阶段。 */
} terror_phase;

/** 一次 Tapas 错误的结构化诊断信息。 */
typedef struct {
	terror_type type;         /**< 具体错误类别。 */
	terror_phase phase;       /**< 错误发生阶段。 */
	const char *reason;       /**< 静态持有的简短原因。 */
	tstring *where;           /**< 可为空的发生位置说明。 */
	tstring *detail;          /**< 可为空的详细信息。 */
	tstring *source;          /**< 可为空的相关源码。 */
	tstring *file;            /**< 可为空的文件路径。 */
	uint64_t line;            /**< 从一开始的行号。 */
	uint64_t column;          /**< 从一开始的列号。 */
	uint_cmds instruction;    /**< 相关字节码位置。 */
	int has_location;         /**< 是否具有有效源码位置。 */
	int has_instruction;      /**< 是否具有有效字节码位置。 */
} terror;

/**
 * @brief 取得错误类别的静态名称。
 *
 * @param t 错误类别。
 * @return 静态只读名称。
 */
const char *terror_type_name(terror_type t);

/**
 * @brief 取得错误阶段的静态名称。
 *
 * @param p 错误阶段。
 * @return 静态只读名称。
 */
const char *terror_phase_name(terror_phase p);

/**
 * @brief 取得当前线程最近一次错误。
 *
 * @return 内部持有的只读指针；尚无错误时返回空指针。
 */
const terror *terror_last(void);

/** 清除当前线程保存的最近错误。 */
void terror_clear_last(void);

/**
 * @brief 复制并设置当前源码上下文。
 *
 * @param source 以空字符结尾的源码。
 */
void terror_set_source_context(const char *source);

/**
 * @brief 借用并设置当前源码上下文。
 *
 * @param source 生命周期覆盖上下文使用期的源码指针。
 */
void terror_set_source_context_borrowed(const char *source);

/** 清除当前源码上下文。 */
void terror_clear_source_context(void);

/**
 * @brief 取得当前源码上下文。
 *
 * @return 内部持有或借用的只读源码指针；未设置时返回空指针。
 */
const char *terror_current_source_context(void);

/**
 * @brief 复制并设置当前文件位置上下文。
 *
 * @param file 文件路径。
 * @param line 从一开始的行号。
 * @param column 从一开始的列号。
 */
void terror_set_file_context(const char *file, uint64_t line, uint64_t column);

/**
 * @brief 借用并设置当前文件位置上下文。
 *
 * @param file 生命周期覆盖上下文使用期的文件路径。
 * @param line 从一开始的行号。
 * @param column 从一开始的列号。
 */
void terror_set_file_context_borrowed(const char *file, uint64_t line, uint64_t column);

/** 清除当前文件位置上下文。 */
void terror_clear_file_context(void);

/**
 * @brief 取得当前文件路径上下文。
 *
 * @return 内部持有或借用的只读路径；未设置时返回空指针。
 */
const char *terror_current_file_context(void);

/**
 * @brief 取得当前行号上下文。
 *
 * @return 当前行号；未设置位置时返回零。
 */
uint64_t terror_current_line_context(void);

/**
 * @brief 取得当前列号上下文。
 *
 * @return 当前列号；未设置位置时返回零。
 */
uint64_t terror_current_column_context(void);

/**
 * @brief 设置当前字节码位置上下文。
 *
 * @param instruction 字节码位置。
 */
void terror_set_instruction_context(uint_cmds instruction);

/** 清除当前字节码位置上下文。 */
void terror_clear_instruction_context(void);

/** 发生错误时惰性解析运行时源码位置的回调。 */
typedef void (*terror_runtime_context_resolver)(
	void *context, const char **source, const char **file,
	uint64_t *line, uint64_t *column, uint_cmds *instruction);

/** 可保存并恢复的运行时位置解析器状态。 */
typedef struct {
	terror_runtime_context_resolver resolver; /**< 位置解析回调。 */
	void *context;                            /**< 回调使用的不透明上下文。 */
} terror_runtime_context_state;

/**
 * @brief 安装惰性的运行时源码位置解析器。
 *
 * @param resolver 发生错误时调用的位置解析器。
 * @param context 原样传给解析器的不透明上下文。
 * @return 安装前的状态，可交给 terror_restore_runtime_context_resolver() 恢复。
 */
terror_runtime_context_state terror_set_runtime_context_resolver(
	terror_runtime_context_resolver resolver, void *context);

/**
 * @brief 恢复先前保存的运行时位置解析器状态。
 *
 * @param state 先前安装操作返回的状态。
 */
void terror_restore_runtime_context_resolver(
	terror_runtime_context_state state);

/**
 * @brief 记录并报告指定类别的错误或警告。
 *
 * @param type 错误类别。
 * @param fname 发生错误的功能名称。
 * @param info 补充说明。
 */
void twarn(terror_type type, const char *fname, const char *info);

/** 交互式 CLI 启用恢复时使用的跳转点。 */
extern jmp_buf tapas_error_jmpbuf;

/** 非零时允许错误处理跳转到 `tapas_error_jmpbuf`。 */
extern int tapas_error_recover_enabled;

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_TBASIS_H */
