#define _POSIX_C_SOURCE 200809L
#include "system.h"
#include "tapas/objects/tstr.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int tstdlib_fail(tstdlib_error *error, const char *code, const char *message)
{
	error->code = code;
	snprintf(error->message, sizeof(error->message), "%s", message);
	return 0;
}

int tstdlib_syserr(tstdlib_error *error, int number, const char *operation)
{
	if (!number)
		number = EIO;
	error->system_errno = number;
	const char *code = "io_error";
	switch (number) {
	case ENOENT:
		code = "not_found";
		break;
	case EACCES:
	case EPERM:
		code = "permission_denied";
		break;
	case EISDIR:
		code = "is_directory";
		break;
	case ENOTDIR:
		code = "not_directory";
		break;
	case ENOSPC:
		code = "no_space";
		break;
	case EEXIST:
		code = "already_exists";
		break;
	case ENOTEMPTY:
		code = "not_empty";
		break;
	case ELOOP:
		code = "symlink_loop";
		break;
	case EXDEV:
		code = "cross_device";
		break;
	}
	error->code = code;
	snprintf(error->message, sizeof(error->message), "%s: %s", operation, strerror(number));
	return 0;
}
const char *tstdlib_path(const tobj *value, tstdlib_error *error)
{
	if (value->type != tcompo || tobj_compo_type(value) != compo_tstr) {
		tstdlib_fail(error, "invalid_type", "String path required");
		return nullptr;
	}
	tstr *string = (tstr *)value->val.v_tcompo;
	const char *text = tstring_cstr(string->data);
	size_t n = tstring_len(string->data);
	if (!n || memchr(text, 0, n)) {
		tstdlib_fail(error, "invalid_path", "path must be nonempty and contain no NUL");
		return nullptr;
	}
	return text;
}

void tstdlib_field(tdict *record, const char *name, const tobj *value)
{
	tobj key = {0};
	tstr *text = tstr_new(name);
	if (!text)
		twarn(ErrRuntime_Other, "result field", "out of memory");
	tobj_set_compo(&key, (tcompo_v *)text);
	tdict_set(record, &key, value);
	tobj_try_clear(&key);
}

void tstdlib_field_string(tdict *record, const char *name, const char *value)
{
	tobj item = {0};
	tstr *text = tstr_new(value);
	if (!text)
		twarn(ErrRuntime_Other, "result field", "out of memory");
	tobj_set_compo(&item, (tcompo_v *)text);
	tstdlib_field(record, name, &item);
	tobj_try_clear(&item);
}

void tstdlib_field_int(tdict *record, const char *name, long value)
{
	tobj item = {0};
	tobj_set_int(&item, value);
	tstdlib_field(record, name, &item);
}

void tstdlib_field_bool(tdict *record, const char *name, int value)
{
	tobj item = {0};
	tobj_set_bool(&item, value);
	tstdlib_field(record, name, &item);
}

void tstdlib_field_float(tdict *record, const char *name, double value)
{
	tobj item = {0};
	tobj_set_float(&item, value);
	tstdlib_field(record, name, &item);
}

int tstdlib_monotonic(double *value)
{
	struct timespec time;
	if (clock_gettime(CLOCK_MONOTONIC, &time))
		return 0;
	*value = (double)time.tv_sec + time.tv_nsec / 1e9;
	return 1;
}
