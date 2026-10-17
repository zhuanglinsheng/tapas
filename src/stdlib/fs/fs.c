#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "tapas/textension.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/ttime.h"
#include "../system.h"
#include "../text.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum {
	fs_stat,
	fs_lstat,
	fs_exists,
	fs_list,
	fs_mkdir,
	fs_copy,
	fs_rename,
	fs_remove,
	fs_temp_dir,
	fs_temp_file
} operation;

static int metadata(const char *path, int follow, tobj *result, tstdlib_error *error)
{
	struct stat info;
	if ((follow ? stat(path, &info) : lstat(path, &info)) != 0)
		return tstdlib_syserr(error, errno, "stat");
	if (info.st_size < 0 || (uintmax_t)info.st_size > LONG_MAX)
		return tstdlib_fail(error, "out_of_range", "file size exceeds Int");
	tdict *record = tdict_new_sized(4);
	tobj_set_compo(result, (tcompo_v *)record);
	tstdlib_field_string(record, "kind",
	    S_ISREG(info.st_mode)   ? "file"
	    : S_ISDIR(info.st_mode) ? "directory"
	    : S_ISLNK(info.st_mode) ? "symlink"
				    : "other");
	tstdlib_field_int(record, "size", (long)info.st_size);
	tstdlib_field_int(record, "permissions", info.st_mode & 07777);
	tobj modified = {0};
	tobj_set_compo(&modified, (tcompo_v *)ttime_from_time(info.st_mtime));
	tstdlib_field(record, "modified", &modified);
	tobj_try_clear(&modified);
	return 1;
}

static int compare_names(const void *a, const void *b)
{
	int result = 0;
	tobj_compare(a, b, &result);
	return result;
}

static int list_directory(const char *path, tobj *result, tstdlib_error *error)
{
	DIR *directory = opendir(path);
	if (!directory)
		return tstdlib_syserr(error, errno, "opendir");
	tlist *names = tlist_new();
	tobj_set_compo(result, (tcompo_v *)names);
	for (;;) {
		errno = 0;
		struct dirent *entry = readdir(directory);
		if (!entry) {
			if (errno)
				tstdlib_syserr(error, errno, "readdir");
			break;
		}
		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
			continue;
		tobj name = {0};
		tstr *string = tstr_new(entry->d_name);
		if (!string) {
			closedir(directory);
			tobj_try_clear(result);
			twarn(ErrRuntime_Other, "fs::list_dir", "out of memory");
		}
		tobj_set_compo(&name, (tcompo_v *)string);
		tobj_vec_push(&names->items, &name);
		tobj_try_clear(&name);
	}
	if (closedir(directory) && !error->code)
		tstdlib_syserr(error, errno, "closedir");
	if (error->code) {
		tobj_try_clear(result);
		tobj_set_compo(result, (tcompo_v *)tlist_new());
		return 0;
	}
	if (names->items.len > 1)
		qsort(names->items.data, names->items.len, sizeof(tobj), compare_names);
	return 1;
}

static int make_directory(const char *path, int parents, tstdlib_error *error)
{
	if (!parents)
		return mkdir(path, 0777) == 0 ? 1 : tstdlib_syserr(error, errno, "mkdir");
	char *copy = strdup(path);
	if (!copy)
		twarn(ErrRuntime_Other, "fs::mkdir", "out of memory");
	for (size_t i = 1;; i++) {
		if (copy[i] && copy[i] != '/')
			continue;
		char saved = copy[i];
		copy[i] = 0;
		if (mkdir(copy, 0777) != 0) {
			int number = errno;
			struct stat info;
			if (number != EEXIST || stat(copy, &info) != 0 || !S_ISDIR(info.st_mode)) {
				if (number == EEXIST)
					number = ENOTDIR;
				free(copy);
				return tstdlib_syserr(error, number, "mkdir");
			}
		}
		copy[i] = saved;
		if (!saved)
			break;
	}
	free(copy);
	return 1;
}

static int copy_file(const char *source, const char *target, tstdlib_error *error)
{
	int input = open(source, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
	if (input < 0)
		return tstdlib_syserr(error, errno, "open source");
	struct stat info;
	if (fstat(input, &info) != 0) {
		tstdlib_syserr(error, errno, "stat source");
		close(input);
		return 0;
	}
	if (!S_ISREG(info.st_mode)) {
		close(input);
		return tstdlib_fail(error, "invalid_type", "copy requires a regular source file");
	}
	int output = open(target, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
	if (output < 0) {
		tstdlib_syserr(error, errno, "create target");
		close(input);
		return 0;
	}
	char buffer[65536];
	for (;;) {
		ssize_t n = read(input, buffer, sizeof(buffer));
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0) {
			tstdlib_syserr(error, errno, "read source");
			break;
		}
		if (!n)
			break;
		for (ssize_t offset = 0; offset < n;) {
			ssize_t count = write(output, buffer + offset, (size_t)(n - offset));
			if (count < 0 && errno == EINTR)
				continue;
			if (count <= 0) {
				tstdlib_syserr(error, errno, "write target");
				break;
			}
			offset += count;
		}
		if (error->code)
			break;
	}
	if (close(input) && !error->code)
		tstdlib_syserr(error, errno, "close source");
	if (close(output) && !error->code)
		tstdlib_syserr(error, errno, "close target");
	if (error->code)
		unlink(target);
	return !error->code;
}

static int temporary(const char *parent, int directory, tobj *result, tstdlib_error *error)
{
	size_t n = strlen(parent);
	if (n > SIZE_MAX - 32)
		return tstdlib_fail(error, "invalid_path", "path too large");
	char *path = malloc(n + 32);
	if (!path)
		twarn(ErrRuntime_Other, "fs temporary", "out of memory");
	memcpy(path, parent, n);
	strcpy(path + n, "/tapas-XXXXXX");
	int fd = -1;
	if (directory ? mkdtemp(path) == nullptr : (fd = mkstemp(path)) < 0) {
		tstdlib_syserr(error, errno, "create temporary");
		free(path);
		return 0;
	}
	if (fd >= 0 && close(fd) != 0) {
		tstdlib_syserr(error, errno, "close temporary");
		unlink(path);
		free(path);
		return 0;
	}
	tstr *value = tstr_new(path);
	if (!value) {
		if (directory)
			rmdir(path);
		else
			unlink(path);
		free(path);
		twarn(ErrRuntime_Other, "fs temporary", "out of memory");
	}
	free(path);
	tobj_set_compo(result, (tcompo_v *)value);
	return 1;
}

static int changes_filesystem(operation op)
{
	switch (op) {
	case fs_mkdir:
	case fs_copy:
	case fs_rename:
	case fs_remove:
		return 1;
	default:
		return 0;
	}
}

/* A failed try_ call always returns an empty value of the documented shape. */

static void initialize_value(operation op, tobj *value)
{
	switch (op) {
	case fs_stat:
	case fs_lstat:
		tobj_set_compo(value, (tcompo_v *)tdict_new());
		break;
	case fs_list:
		tobj_set_compo(value, (tcompo_v *)tlist_new());
		break;
	case fs_temp_dir:
	case fs_temp_file:
		tobj_set_compo(value, (tcompo_v *)tstr_new(""));
		break;
	default:
		tobj_set_bool(value, 0);
	}
}

static int execute_operation(operation op, const char *path, const char *target, int parents,
    tobj *value, tstdlib_error *error)
{
	struct stat info;
	switch (op) {
	case fs_stat:
	case fs_lstat:
		return metadata(path, op == fs_stat, value, error);
	case fs_exists:
		if (stat(path, &info) == 0) {
			tobj_set_bool(value, 1);
			return 1;
		}
		if (errno == ENOENT || errno == ENOTDIR)
			return 1;
		return tstdlib_syserr(error, errno, "stat");
	case fs_list:
		return list_directory(path, value, error);
	case fs_mkdir:
		return make_directory(path, parents, error);
	case fs_copy:
		return copy_file(path, target, error);
	case fs_rename:
		return rename(path, target) == 0 ? 1 : tstdlib_syserr(error, errno, "rename");
	case fs_remove:
		if (lstat(path, &info) != 0)
			return tstdlib_syserr(error, errno, "lstat");
		if ((S_ISDIR(info.st_mode) ? rmdir(path) : unlink(path)) != 0)
			return tstdlib_syserr(error, errno, "remove");
		return 1;
	case fs_temp_dir:
	case fs_temp_file:
		return temporary(path, op == fs_temp_dir, value, error);
	}
	return 0;
}

static void perform(
    tobj *params, uint_regs count, tobj *result, operation op, int attempt, const char *where)
{
	tstdlib_error error = {0};
	const char *path;
	const char *target = nullptr;
	if ((op == fs_temp_dir || op == fs_temp_file) && !count) {
		path = getenv("TMPDIR");
		if (!path || !*path)
			path = "/tmp";
	} else {
		path = tstdlib_path(&params[0], &error);
	}
	if (!error.code && (op == fs_copy || op == fs_rename))
		target = tstdlib_path(&params[1], &error);
	int parents = 0;
	if (!error.code && op == fs_mkdir && count == 2) {
		if (params[1].type != tbool)
			tstdlib_fail(&error, "invalid_type", "parents must be Bool");
		else
			parents = params[1].val.v_tbool;
	}

	tobj value = {0};
	initialize_value(op, &value);
	if (!error.code) {
		int ok = execute_operation(op, path, target, parents, &value, &error);
		if (changes_filesystem(op))
			tobj_set_bool(&value, ok);
	}
	if (attempt) {
		tstdlib_make_result(result, &value, &error);
		tobj_try_clear(&value);
	} else if (error.code) {
		tobj_try_clear(&value);
		twarn(error.system_errno ? ErrSession_IO : ErrRuntime_ParamsType, where,
		    error.message);
	} else if (changes_filesystem(op)) {
		tobj_set_nil(result);
	} else if (value.type == tcompo) {
		/* Transfer ownership; do not destroy the object now owned by result. */
		tobj_set_compo(result, value.val.v_tcompo);
		tobj_set_nil(&value);
	} else {
		tobj_copy(result, &value);
	}
}

static void fs_metadata(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_stat, 0, "fs::stat");
}

static void fs_link_metadata(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_lstat, 0, "fs::lstat");
}

static void fs_path_exists(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_exists, 0, "fs::exists");
}

static void fs_directory_entries(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_list, 0, "fs::list_dir");
}

static void fs_create_directory(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_mkdir, 0, "fs::mkdir");
}

static void fs_copy_file(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_copy, 0, "fs::copy");
}

static void fs_rename_path(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_rename, 0, "fs::rename");
}

static void fs_remove_path(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_remove, 0, "fs::remove");
}

static void fs_temporary_directory(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_temp_dir, 0, "fs::temp_dir");
}

static void fs_temporary_file(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_temp_file, 0, "fs::temp_file");
}

static void fs_attempt_stat(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_stat, 1, "fs::try_stat");
}

static void fs_attempt_lstat(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_lstat, 1, "fs::try_lstat");
}

static void fs_attempt_exists(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_exists, 1, "fs::try_exists");
}

static void fs_attempt_list_dir(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_list, 1, "fs::try_list_dir");
}

static void fs_attempt_mkdir(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_mkdir, 1, "fs::try_mkdir");
}

static void fs_attempt_copy(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_copy, 1, "fs::try_copy");
}

static void fs_attempt_rename(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_rename, 1, "fs::try_rename");
}

static void fs_attempt_remove(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_remove, 1, "fs::try_remove");
}

static void fs_attempt_temp_dir(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_temp_dir, 1, "fs::try_temp_dir");
}

static void fs_attempt_temp_file(tobj *p, uint_regs n, tobj *r)
{
	perform(p, n, r, fs_temp_file, 1, "fs::try_temp_file");
}

static const textension_symbol symbols[] = {
    {.name = "stat",
	.type = "Function[String] -> Dictionary",
	.detail = "fs::stat(path: String) -> Dictionary",
	.kind = textension_function,
	.function = fs_metadata,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "lstat",
	.type = "Function[String] -> Dictionary",
	.detail = "fs::lstat(path: String) -> Dictionary",
	.kind = textension_function,
	.function = fs_link_metadata,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "exists",
	.type = "Function[String] -> Bool",
	.detail = "fs::exists(path: String) -> Bool",
	.kind = textension_function,
	.function = fs_path_exists,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "list_dir",
	.type = "Function[String] -> List",
	.detail = "fs::list_dir(path: String) -> List",
	.kind = textension_function,
	.function = fs_directory_entries,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "mkdir",
	.type = "Function[...] -> Nil",
	.detail = "fs::mkdir(path: String, [parents: Bool]) -> Nil",
	.kind = textension_function,
	.function = fs_create_directory,
	.minimum_arguments = 1,
	.maximum_arguments = 2},
    {.name = "copy",
	.type = "Function[String, String] -> Nil",
	.detail = "fs::copy(path: String, target: String) -> Nil",
	.kind = textension_function,
	.function = fs_copy_file,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "rename",
	.type = "Function[String, String] -> Nil",
	.detail = "fs::rename(path: String, target: String) -> Nil",
	.kind = textension_function,
	.function = fs_rename_path,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "remove",
	.type = "Function[String] -> Nil",
	.detail = "fs::remove(path: String) -> Nil",
	.kind = textension_function,
	.function = fs_remove_path,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "temp_dir",
	.type = "Function[...] -> String",
	.detail = "fs::temp_dir([parent: String]) -> String",
	.kind = textension_function,
	.function = fs_temporary_directory,
	.minimum_arguments = 0,
	.maximum_arguments = 1},
    {.name = "temp_file",
	.type = "Function[...] -> String",
	.detail = "fs::temp_file([parent: String]) -> String",
	.kind = textension_function,
	.function = fs_temporary_file,
	.minimum_arguments = 0,
	.maximum_arguments = 1},
    {.name = "try_stat",
	.type = "Function[AnyType] -> Dictionary",
	.detail = "fs::try_stat(path: AnyType) -> Dictionary",
	.kind = textension_function,
	.function = fs_attempt_stat,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "try_lstat",
	.type = "Function[AnyType] -> Dictionary",
	.detail = "fs::try_lstat(path: AnyType) -> Dictionary",
	.kind = textension_function,
	.function = fs_attempt_lstat,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "try_exists",
	.type = "Function[AnyType] -> Dictionary",
	.detail = "fs::try_exists(path: AnyType) -> Dictionary",
	.kind = textension_function,
	.function = fs_attempt_exists,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "try_list_dir",
	.type = "Function[AnyType] -> Dictionary",
	.detail = "fs::try_list_dir(path: AnyType) -> Dictionary",
	.kind = textension_function,
	.function = fs_attempt_list_dir,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "try_mkdir",
	.type = "Function[...] -> Dictionary",
	.detail = "fs::try_mkdir(path: AnyType, [parents: Bool]) -> Dictionary",
	.kind = textension_function,
	.function = fs_attempt_mkdir,
	.minimum_arguments = 1,
	.maximum_arguments = 2},
    {.name = "try_copy",
	.type = "Function[AnyType, AnyType] -> Dictionary",
	.detail = "fs::try_copy(path: AnyType, target: AnyType) -> Dictionary",
	.kind = textension_function,
	.function = fs_attempt_copy,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "try_rename",
	.type = "Function[AnyType, AnyType] -> Dictionary",
	.detail = "fs::try_rename(path: AnyType, target: AnyType) -> Dictionary",
	.kind = textension_function,
	.function = fs_attempt_rename,
	.minimum_arguments = 2,
	.maximum_arguments = 2},
    {.name = "try_remove",
	.type = "Function[AnyType] -> Dictionary",
	.detail = "fs::try_remove(path: AnyType) -> Dictionary",
	.kind = textension_function,
	.function = fs_attempt_remove,
	.minimum_arguments = 1,
	.maximum_arguments = 1},
    {.name = "try_temp_dir",
	.type = "Function[...] -> Dictionary",
	.detail = "fs::try_temp_dir([parent: String]) -> Dictionary",
	.kind = textension_function,
	.function = fs_attempt_temp_dir,
	.minimum_arguments = 0,
	.maximum_arguments = 1},
    {.name = "try_temp_file",
	.type = "Function[...] -> Dictionary",
	.detail = "fs::try_temp_file([parent: String]) -> Dictionary",
	.kind = textension_function,
	.function = fs_attempt_temp_file,
	.minimum_arguments = 0,
	.maximum_arguments = 1},
};
const textension_module tstdlib_fs_module = {.scope = textension_package,
    .name = "fs",
    .detail = "Filesystem operations with recoverable counterparts",
    .symbols = symbols,
    .symbol_count = sizeof(symbols) / sizeof(symbols[0])};
