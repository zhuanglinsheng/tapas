#include "result.h"
#include "tapas/objects/tdict.h"
#include "tapas/objects/tstr.h"

static void set_field(tdict *record, const char *name, const tobj *value)
{
	tobj key = {0};
	tstr *string = tstr_new(name);
	if (!string)
		twarn(ErrRuntime_Other, "result", "out of memory");
	tobj_set_compo(&key, (tcompo_v *)string);
	tdict_set(record, &key, value);
	tobj_try_clear(&key);
}

static void string_field(tdict *record, const char *name, const char *text)
{
	tobj value = {0};
	tstr *string = tstr_new(text);
	if (!string)
		twarn(ErrRuntime_Other, "result", "out of memory");
	tobj_set_compo(&value, (tcompo_v *)string);
	set_field(record, name, &value);
	tobj_try_clear(&value);
}

void tstdlib_make_result(tobj *result, const tobj *value,
	const tstdlib_error *error)
{
	tdict *record = tdict_new_sized(5);
	tobj_set_compo(result, (tcompo_v *)record);
	tobj field = {0};
	tobj_set_bool(&field, !error->code);
	set_field(record, "ok", &field);
	set_field(record, "value", value);
	string_field(record, "code", error->code ? error->code : "");
	string_field(record, "error", error->message);
	tobj_set_int(&field, error->system_errno);
	set_field(record, "errno", &field);
}
