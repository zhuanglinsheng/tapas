#include "tapas/tbasis.h"

/*===========================================================================*
 * Error System
 *===========================================================================*/

jmp_buf tapas_error_jmpbuf;
int tapas_error_recover_enabled = 0;

static terror tapas_last_error;
static tstring *tapas_error_source_ctx = NULL;
static tstring *tapas_error_file_ctx = NULL;
static const char *tapas_error_source_view = NULL;
static const char *tapas_error_file_view = NULL;
static uint64_t tapas_error_line_ctx = 0;
static uint64_t tapas_error_column_ctx = 0;
static int tapas_error_has_file_ctx = 0;
static uint_cmds tapas_error_instruction_ctx = 0;
static int tapas_error_has_instruction_ctx = 0;
static terror_runtime_context_resolver tapas_runtime_context_resolver = NULL;
static void *tapas_runtime_context_data = NULL;

static void terror_reset(terror *err)
{
	if (!err)
		return;
	tstring_free(err->where);
	tstring_free(err->detail);
	tstring_free(err->source);
	tstring_free(err->file);
	memset(err, 0, sizeof(*err));
	err->phase = TErrorPhase_Unknown;
}

static const char *terror_reason(terror_type t)
{
	const char *name = terror_type_name(t);
	const char *sep = strstr(name, " - ");
	return sep ? sep + 3 : name;
}

static terror_phase terror_phase_from_type(terror_type t)
{
	if (t >= ErrCompile_Other && t <= ErrCompile_InvalidFile)
		return TErrorPhase_Compile;
	if (t == ErrSession_IO)
		return TErrorPhase_Session;
	if (t >= ErrRuntime_Other && t <= ErrRuntime_RecurseRefRet)
		return TErrorPhase_Runtime;
	return TErrorPhase_Unknown;
}

const char *terror_type_name(terror_type t)
{
	switch (t) {
	case ErrCompile_Other:
		return "Compile Error";
	case ErrCompile_UnfoundFile:
		return "Compile Error - Unfound File";
	case ErrCompile_BracketsOpen:
		return "Compile Error - Bracket Open";
	case ErrCompile_VarNoType:
		return "Compile Error - Variable Declaration of no Type";
	case ErrCompile_DblVDeclare:
		return "Compile Error - Duplicate Variable Declaration";
	case ErrCompile_InBlkVarDef:
		return "Compile Error - Variable Declaration in Block";
	case ErrCompile_ObjUnfound:
		return "Compile Error - Object Unfound";
	case ErrCompile_InvalidVname:
		return "Compile Error - Invalid Variable Name";
	case ErrCompile_InvalidLiter:
		return "Compile Error - Invalid Literal Value";
	case ErrCompile_AsgDefault:
		return "Compile Error - Assign Values to Defaults";
	case ErrCompile_REGOutOfLimit:
		return "Compile Error - Parameters Overflow";
	case ErrCompile_CMDOutOfLimit:
		return "Compile Error - Command Overflow";
	case ErrCompile_OBJOutOfLimit:
		return "Compile Error - Variable Overflow";
	case ErrCompile_CSTOutOfLimit:
		return "Compile Error - Constants Overflow";
	case ErrCompile_ReturnTmpObj:
		return "Compile Error - Return Temporary Object";
	case ErrCompile_InvalidFile:
		return "Compile Error - Invalid File Name/Suffix";

	case ErrSession_IO:
		return "Session Error - IO";

	case ErrRuntime_Other:
		return "Runtime Error";
	case ErrRuntime_DivIntZero:
		return "Runtime Error - Divided by Integer Zero";
	case ErrRuntime_ParamsCtr:
		return "Runtime Error - Parameters Count Inconsistency";
	case ErrRuntime_ParamsType:
		return "Runtime Error - Parameters Type Inconsistency";
	case ErrRuntime_IdxOutRange:
		return "Runtime Error - Index out of Range";
	case ErrRuntime_InvalidIndex:
		return "Runtime Error - Invalid Index";
	case ErrRuntime_LoopRef:
		return "Runtime Error - Looping Reference";
	case ErrRuntime_RefType:
		return "Runtime Error - Referred Type Inconsistency";
	case ErrRuntime_LenInconsis:
		return "Runtime Error - Length Inconsistency";
	case ErrRuntime_AssignNil:
		return "Runtime Error - Try to Assign Nil";
	case ErrRuntime_ObjUnfound:
		return "Runtime Error - Object Unfound";
	case ErrRuntime_IntOutOfRange:
		return "Runtime Error - Integer Value out of Range";
	case ErrRuntime_RefEmptySet:
		return "Runtime Error - Refer to the Value of Empty Set";
	case ErrRuntime_StringEval:
		return "Runtime Error - String Evaluation";
	case ErrRuntime_EnvInconsis:
		return "Runtime Error - Environment Inconsistecy";
	case ErrRuntime_RecurseRefRet:
		return "Runtime Error - Return Local Reference in Recursion";

	default:
		return "Unknown Error";
	}
}

const char *terror_phase_name(terror_phase p)
{
	switch (p) {
	case TErrorPhase_Compile:
		return "Compile Error";
	case TErrorPhase_Runtime:
		return "Runtime Error";
	case TErrorPhase_Session:
		return "Session Error";
	case TErrorPhase_Unknown:
	default:
		return "Unknown Error";
	}
}

const terror *terror_last(void)
{
	return &tapas_last_error;
}

void terror_clear_last(void)
{
	terror_reset(&tapas_last_error);
}

void terror_set_source_context(const char *source)
{
	tstring_free(tapas_error_source_ctx);
	tapas_error_source_ctx = source ? tstring_new(source) : NULL;
	tapas_error_source_view = NULL;
}

void terror_set_source_context_borrowed(const char *source)
{
	tstring_free(tapas_error_source_ctx);
	tapas_error_source_ctx = NULL;
	tapas_error_source_view = source;
}

void terror_clear_source_context(void)
{
	tstring_free(tapas_error_source_ctx);
	tapas_error_source_ctx = NULL;
	tapas_error_source_view = NULL;
}

const char *terror_current_source_context(void)
{
	return tapas_error_source_ctx ? tstring_cstr(tapas_error_source_ctx) :
	       tapas_error_source_view;
}

void terror_set_file_context(const char *file, uint64_t line, uint64_t column)
{
	tstring_free(tapas_error_file_ctx);
	tapas_error_file_ctx = file ? tstring_new(file) : NULL;
	tapas_error_file_view = NULL;
	tapas_error_line_ctx = line;
	tapas_error_column_ctx = column;
	tapas_error_has_file_ctx = file != NULL || line != 0 || column != 0;
}

void terror_set_file_context_borrowed(const char *file, uint64_t line, uint64_t column)
{
	tstring_free(tapas_error_file_ctx);
	tapas_error_file_ctx = NULL;
	tapas_error_file_view = file;
	tapas_error_line_ctx = line;
	tapas_error_column_ctx = column;
	tapas_error_has_file_ctx = file != NULL || line != 0 || column != 0;
}

void terror_clear_file_context(void)
{
	tstring_free(tapas_error_file_ctx);
	tapas_error_file_ctx = NULL;
	tapas_error_file_view = NULL;
	tapas_error_line_ctx = 0;
	tapas_error_column_ctx = 0;
	tapas_error_has_file_ctx = 0;
}

const char *terror_current_file_context(void)
{
	return tapas_error_file_ctx ? tstring_cstr(tapas_error_file_ctx) :
	       tapas_error_file_view;
}

uint64_t terror_current_line_context(void)
{
	return tapas_error_line_ctx;
}

uint64_t terror_current_column_context(void)
{
	return tapas_error_column_ctx;
}

void terror_set_instruction_context(uint_cmds instruction)
{
	tapas_error_instruction_ctx = instruction;
	tapas_error_has_instruction_ctx = 1;
}

void terror_clear_instruction_context(void)
{
	tapas_error_instruction_ctx = 0;
	tapas_error_has_instruction_ctx = 0;
}

terror_runtime_context_state terror_set_runtime_context_resolver(
	terror_runtime_context_resolver resolver, void *context)
{
	terror_runtime_context_state previous = {
		.resolver = tapas_runtime_context_resolver,
		.context = tapas_runtime_context_data
	};
	tapas_runtime_context_resolver = resolver;
	tapas_runtime_context_data = context;
	return previous;
}

void terror_restore_runtime_context_resolver(
	terror_runtime_context_state state)
{
	tapas_runtime_context_resolver = state.resolver;
	tapas_runtime_context_data = state.context;
}

static void terror_capture(terror_type type, const char *fname, const char *info)
{
	terror_reset(&tapas_last_error);
	tapas_last_error.type = type;
	tapas_last_error.phase = terror_phase_from_type(type);
	tapas_last_error.reason = terror_reason(type);
	tapas_last_error.where = tstring_new(fname ? fname : "");
	tapas_last_error.detail = tstring_new(info ? info : "");
	const char *runtime_source = NULL;
	const char *runtime_file = NULL;
	uint64_t runtime_line = 0;
	uint64_t runtime_column = 0;
	uint_cmds runtime_instruction = 0;
	terror_runtime_context_resolver resolver =
		tapas_runtime_context_resolver;
	void *resolver_context = tapas_runtime_context_data;
	if (resolver) {
		/* An error may leave through longjmp, so do not retain a pointer to
		 * the resolver's stack-allocated context after resolving it. */
		tapas_runtime_context_resolver = NULL;
		tapas_runtime_context_data = NULL;
		resolver(resolver_context, &runtime_source, &runtime_file,
			 &runtime_line, &runtime_column, &runtime_instruction);
	}
	const char *source_ctx = runtime_source ? runtime_source :
		terror_current_source_context();
	const char *file_ctx = runtime_file ? runtime_file :
		terror_current_file_context();
	if (source_ctx)
		tapas_last_error.source = tstring_new(source_ctx);
	if (file_ctx)
		tapas_last_error.file = tstring_new(file_ctx);
	if (resolver) {
		tapas_last_error.line = runtime_line;
		tapas_last_error.column = runtime_column;
		tapas_last_error.has_location = file_ctx != NULL ||
			runtime_line != 0 || runtime_column != 0;
		tapas_last_error.instruction = runtime_instruction;
		tapas_last_error.has_instruction = 1;
	} else {
		tapas_last_error.line = tapas_error_line_ctx;
		tapas_last_error.column = tapas_error_column_ctx;
		tapas_last_error.has_location = tapas_error_has_file_ctx;
		tapas_last_error.instruction = tapas_error_instruction_ctx;
		tapas_last_error.has_instruction = tapas_error_has_instruction_ctx;
	}
}

static void terror_print(const terror *err)
{
	fprintf(stderr, "%s: %s\n", terror_phase_name(err->phase), err->reason);
	if (err->has_location) {
		fprintf(stderr, "  at ");
		if (err->file && !tstring_empty(err->file))
			fprintf(stderr, "%s", tstring_cstr(err->file));
		if (err->line)
			fprintf(stderr, ":%llu", (unsigned long long)err->line);
		if (err->column)
			fprintf(stderr, ":%llu", (unsigned long long)err->column);
		fprintf(stderr, "\n");
	}
	int detail_marked = 0;
	if (err->source && !tstring_empty(err->source)) {
		const char *src = tstring_cstr(err->source);
		size_t src_line_len = strcspn(src, "\r\n");
		const char *detail = err->detail ? tstring_cstr(err->detail) : "";
		const char *hit = detail[0] ? strstr(src, detail) : NULL;
		fprintf(stderr, "  code:\n");
		fprintf(stderr, "    %.*s\n", (int)src_line_len, src);
		if (hit) {
			size_t off = (size_t)(hit - src);
			size_t len = strlen(detail);
			if (off + len > src_line_len)
				len = off < src_line_len ? src_line_len - off : 0;
			fprintf(stderr, "    ");
			for (size_t i = 0; i < off; i++)
				fputc(src[i] == '\t' ? '\t' : ' ', stderr);
			for (size_t i = 0; i < len; i++)
				fputc('^', stderr);
			fprintf(stderr, "\n");
			detail_marked = 1;
		} else {
			size_t line_start = 0;
			size_t line_end = strcspn(src, "\r\n");
			while (line_start < line_end &&
			       (src[line_start] == ' ' || src[line_start] == '\t'))
				line_start++;
			while (line_end > line_start &&
			       (src[line_end - 1] == ' ' || src[line_end - 1] == '\t'))
				line_end--;
			if (line_end > line_start) {
				fprintf(stderr, "    ");
				for (size_t i = 0; i < line_start; i++)
					fputc(src[i] == '\t' ? '\t' : ' ', stderr);
				for (size_t i = line_start; i < line_end; i++)
					fputc('^', stderr);
				fprintf(stderr, "\n");
			}
		}
	}
	if (!detail_marked && err->detail && !tstring_empty(err->detail))
		fprintf(stderr, "  detail: %s\n", tstring_cstr(err->detail));
	if (err->where && !tstring_empty(err->where))
		fprintf(stderr, "  from: %s\n", tstring_cstr(err->where));
	if (err->has_instruction)
		fprintf(stderr,
			"  instruction: %u\n",
			(unsigned)err->instruction);
}

/**
 * Emit a warning with type, function name, and info string.
 * In C, this prints the error and calls exit(-1).
 * Functions that call twarn should handle cleanup before calling.
 */
void twarn(terror_type type, const char *fname, const char *info)
{
	fflush(stdout);
	terror_capture(type, fname, info);
	terror_print(&tapas_last_error);
	fflush(stderr);
	if (tapas_error_recover_enabled)
		longjmp(tapas_error_jmpbuf, 1);
	exit(-1);
}
