#include "compile/cache.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static tstring *cache_dirname(const tstring *path)
{
	size_t slash = tstring_rfind_c(path, '/');
	if (slash == SIZE_MAX)
		return tstring_new(".");
	if (slash == 0)
		return tstring_new("/");
	return tstring_substr(path, 0, slash);
}

static const char *cache_basename(const char *path)
{
	const char *slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

static int cache_path_is_below(const char *root, const char *path,
			       const char **relative)
{
	size_t length = strlen(root);
	if (strncmp(root, path, length) != 0)
		return 0;
	if (path[length] == '\0') {
		*relative = path + length;
		return 1;
	}
	if (length == 1 && root[0] == '/') {
		*relative = path + 1;
		return 1;
	}
	if (path[length] != '/')
		return 0;
	*relative = path + length + 1;
	return 1;
}

static uint64_t cache_path_hash(const char *path)
{
	uint64_t hash = UINT64_C(14695981039346656037);
	for (const unsigned char *at = (const unsigned char *)path; *at; at++) {
		hash ^= *at;
		hash *= UINT64_C(1099511628211);
	}
	return hash;
}

static void cache_replace_extension(tstring *path, const char *source)
{
	size_t length = strlen(source);
	if (length >= 4 && strcmp(source + length - 4, ".tap") == 0) {
		size_t path_length = tstring_len(path);
		tstring *without_extension = tstring_substr(
			path, 0, path_length >= 4 ? path_length - 4 : path_length);
		tstring_assign_ts(path, without_extension);
		tstring_free(without_extension);
	}
	tstring_append(path, ".tapc");
}

void tcompile_cache_configure(tlib *library, const tstring *entry_source,
			      int interactive)
{
	if (!library || !entry_source)
		return;
	char working_directory[PATH_MAX];
	tstring *source_root = getcwd(
		working_directory, sizeof(working_directory)) ?
		tstring_new(working_directory) : cache_dirname(entry_source);
	tstring *build_root = tstring_dup(source_root);
	if (!tstring_empty(build_root) &&
	    tstring_at(build_root, tstring_len(build_root) - 1) != '/')
		tstring_append_c(build_root, '/');
	tstring_append(build_root, TAPAS_BUILD_DIRECTORY);
	tlib_set_build_roots(library, tstring_cstr(source_root),
			     tstring_cstr(build_root), interactive);
	tstring_free(build_root);
	tstring_free(source_root);
}

tstring *tcompile_cache_path(const tlib *library, const tstring *source)
{
	const tstring *source_root = tlib_get_source_root(library);
	const tstring *build_root = tlib_get_build_root(library);
	if (!source || !source_root || !build_root)
		return nullptr;
	const char *source_text = tstring_cstr(source);
	const char *relative = nullptr;
	tstring *path = tstring_dup(build_root);
	if (!tstring_empty(path) &&
	    tstring_at(path, tstring_len(path) - 1) != '/')
		tstring_append_c(path, '/');
	if (cache_path_is_below(tstring_cstr(source_root), source_text,
				&relative) && relative && *relative) {
		tstring_append(path, relative);
	} else {
		tstring_append(path, "external/");
		tstring_append_fmt(path, "%016" PRIx64 "/",
			cache_path_hash(source_text));
		tstring_append(path, cache_basename(source_text));
	}
	cache_replace_extension(path, source_text);
	return path;
}

static int cache_make_parent_directories(const tstring *file)
{
	size_t length = tstring_len(file);
	char *path = (char *)malloc(length + 1);
	if (!path)
		return 0;
	memcpy(path, tstring_cstr(file), length + 1);
	char *last = strrchr(path, '/');
	if (!last) {
		free(path);
		return 1;
	}
	*last = '\0';
	for (char *at = path + (path[0] == '/' ? 1 : 0);; at++) {
		if (*at != '/' && *at != '\0')
			continue;
		char saved = *at;
		*at = '\0';
		if (*path && mkdir(path, 0777) != 0 && errno != EEXIST) {
			free(path);
			return 0;
		}
		*at = saved;
		if (saved == '\0')
			break;
	}
	int writable = access(path, W_OK) == 0;
	free(path);
	return writable;
}

int tcompile_cache_save(const tlib *library, const tstring *source,
			const twrapper *wrapper)
{
	tstring *path = tcompile_cache_path(library, source);
	if (!path)
		return 0;
	int file_writable = access(tstring_cstr(path), F_OK) != 0 ||
		access(tstring_cstr(path), W_OK) == 0;
	if (!file_writable || !cache_make_parent_directories(path)) {
		tstring_free(path);
		return 0;
	}
	tstring *temporary = tstring_dup(path);
	tstring_append_fmt(temporary, ".tmp.%ld", (long)getpid());
	int saved = tanalyser_save_bin_file(
		wrapper, tstring_cstr(temporary)) == 0 &&
		rename(tstring_cstr(temporary), tstring_cstr(path)) == 0;
	if (!saved)
		(void)remove(tstring_cstr(temporary));
	tstring_free(temporary);
	tstring_free(path);
	return saved;
}

typedef struct {
	const tstring **items;
	uint32_t count;
	uint32_t capacity;
} cache_validation_stack;

static int cache_stack_contains(const cache_validation_stack *stack,
				const tstring *source)
{
	for (uint32_t i = 0; i < stack->count; i++)
		if (tstring_eq(stack->items[i], source))
			return 1;
	return 0;
}

static int cache_stack_push(cache_validation_stack *stack,
			    const tstring *source)
{
	if (stack->count == stack->capacity) {
		uint32_t capacity = stack->capacity ? stack->capacity * 2 : 16;
		const tstring **items = (const tstring **)realloc(
			stack->items, capacity * sizeof(*items));
		if (!items)
			return 0;
		stack->items = items;
		stack->capacity = capacity;
	}
	stack->items[stack->count++] = source;
	return 1;
}

static twrapper *cache_load_current_recursive(
		const tlib *library, const tstring *source,
		cache_validation_stack *stack)
{
	if (cache_stack_contains(stack, source) || !cache_stack_push(stack, source))
		return nullptr;
	tstring *path = tcompile_cache_path(library, source);
	struct stat source_status;
	struct stat cache_status;
	if (!path || stat(tstring_cstr(source), &source_status) != 0 ||
	    stat(tstring_cstr(path), &cache_status) != 0 ||
	    source_status.st_mtime > cache_status.st_mtime ||
	    tlib_get_build_mtime(library) > (int64_t)cache_status.st_mtime) {
		tstring_free(path);
		stack->count--;
		return nullptr;
	}
	twrapper *wrapper = tanalyser_try_load_bin_file(tstring_cstr(path));
	if (!wrapper || wrapper->info.padding_1 !=
			(uint8_t)tlib_get_build_interactive(library)) {
		tanalyser_clean_wrapper(wrapper);
		tstring_free(path);
		stack->count--;
		return nullptr;
	}
	for (uint_cmds i = 0; i < wrapper->ncmds; i++) {
		if (tbycode_ins(wrapper->cmdarr[i]) != OP_IMPORT)
			continue;
		uint_csts index = (uint_csts)tbycode_get_U(wrapper->cmdarr[i]);
		if (index >= wrapper->consts.ncstrs) {
			tanalyser_clean_wrapper(wrapper);
			wrapper = nullptr;
			break;
		}
		const tstring *dependency = wrapper->consts.cstrs[index];
		twrapper *dependency_wrapper = cache_load_current_recursive(
			library, dependency, stack);
		tstring *dependency_path = tcompile_cache_path(library, dependency);
		struct stat dependency_status;
		int dependency_current = dependency_wrapper && dependency_path &&
			stat(tstring_cstr(dependency_path), &dependency_status) == 0 &&
			dependency_status.st_mtime <= cache_status.st_mtime;
		tanalyser_clean_wrapper(dependency_wrapper);
		tstring_free(dependency_path);
		if (!dependency_current) {
			tanalyser_clean_wrapper(wrapper);
			wrapper = nullptr;
			break;
		}
	}
	tstring_free(path);
	stack->count--;
	return wrapper;
}

twrapper *tcompile_cache_load_current(const tlib *library,
				       const tstring *source)
{
	cache_validation_stack stack = { 0 };
	twrapper *wrapper = cache_load_current_recursive(library, source, &stack);
	free(stack.items);
	return wrapper;
}
