#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "tapas/textension.h"
#include "tapas/tstdlib.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/tdict.h"
#include "tapas/objects/tpair.h"
#include "tapas/objects/tstr.h"
#include "tapas/objects/ttime.h"
#include "../../src/stdlib/bytes/buffer.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void call(const char *package, const char *name, tobj *p, uint_regs n, tobj *r)
{
	textension_symbol_ref found;
	assert(textension_find(tstdlib_descriptor(), package, name, &found));
	found.symbol->function(p, n, r);
}
static void text(tobj *value, const char *string)
{
	tobj_set_compo(value, (tcompo_v *)tstr_new(string));
}
static int equal(const tobj *value, const char *s)
{
	assert(value->type == tcompo && tobj_compo_type(value) == compo_tstr);
	tstring *string = ((tstr *)value->val.v_tcompo)->data;
	return tstring_len(string) == strlen(s) && !memcmp(tstring_cstr(string), s, strlen(s));
}
static void field(const tobj *record, const char *name, tobj *out)
{
	tobj key = {0};
	text(&key, name);
	tdict_get((tdict *)record->val.v_tcompo, &key, out);
	tobj_try_clear(&key);
}
static int field_bool(const tobj *record, const char *name)
{
	tobj value = {0};
	field(record, name, &value);
	return value.val.v_tbool;
}
static long field_int(const tobj *record, const char *name)
{
	tobj value = {0};
	field(record, name, &value);
	return value.val.v_tint;
}
static void field_text(const tobj *record, const char *name, const char *expected)
{
	tobj value = {0};
	field(record, name, &value);
	assert(equal(&value, expected));
	tobj_ddc_ref_clear(&value);
}
static void check_time(void)
{
	tobj p[2] = {{0}}, r = {0}, t = {0};
	const char *valid[] = {"1970-01-01T00:00:00Z", "1969-12-31T23:59:59Z",
	    "2000-02-29T12:34:56Z", "0001-01-01T00:00:00Z", "9999-12-31T23:59:59Z"};
	for (unsigned i = 0; i < 5; i++) {
		text(&p[0], valid[i]);
		call("time", "parse_utc", p, 1, &t);
		if (i < 2)
			assert(ttime_get((ttime *)t.val.v_tcompo) == (i ? -1 : 0));
		tobj_copy(&p[0], &t);
		text(&p[1], "%Y-%m-%dT%H:%M:%SZ");
		call("time", "format_utc", p, 2, &r);
		/* Some C strftime implementations omit padding on years before 1000. */
		if (i != 3)
			assert(equal(&r, valid[i]));
		tobj_ddc_ref_clear(&p[0]);
		tobj_set_nil(&t);
	}
	const char *bad[] = {"", "2023-02-29T00:00:00Z", "1900-02-29T00:00:00Z",
	    "2024-04-31T00:00:00Z", "2024-01-01T24:00:00Z", "2024-01-01T00:00:60Z",
	    "0000-01-01T00:00:00Z", "2024-01-01 00:00:00Z"};
	for (unsigned i = 0; i < 8; i++) {
		text(&p[0], bad[i]);
		call("time", "try_parse_utc", p, 1, &r);
		assert(!field_bool(&r, "ok"));
		field_text(&r, "code", "invalid_time");
	}
	call("time", "monotonic_ns", nullptr, 0, &r);
	long first = r.val.v_tint;
	call("time", "monotonic_ns", nullptr, 0, &r);
	assert(r.val.v_tint >= first);
	call("time", "unix_ns", nullptr, 0, &r);
	assert(r.val.v_tint > 1000000000000000000L);
	for (int i = 0; i < 2; i++)
		tobj_ddc_ref_clear(&p[i]);
	tobj_try_clear(&r);
	tobj_try_clear(&t);
}
static void check_environment(void)
{
	tobj p = {0}, r = {0};
	assert(setenv("TAPAS_SYSTEM_TEST_EMPTY", "", 1) == 0);
	text(&p, "TAPAS_SYSTEM_TEST_EMPTY");
	call("os", "getenv", &p, 1, &r);
	assert(field_bool(&r, "found"));
	field_text(&r, "value", "");
	unsetenv("TAPAS_SYSTEM_TEST_EMPTY");
	call("os", "getenv", &p, 1, &r);
	assert(!field_bool(&r, "found"));
	call("os", "cwd", nullptr, 0, &r);
	assert(tstring_len(((tstr *)r.val.v_tcompo)->data));
	call("os", "home", nullptr, 0, &r);
	assert(tstring_len(((tstr *)r.val.v_tcompo)->data));
	call("os", "platform", nullptr, 0, &r);
	assert(equal(&r, "macos") || equal(&r, "linux"));
	tobj_try_clear(&p);
	tobj_try_clear(&r);
}
static void check_fs(void)
{
	tobj p[2] = {{0}}, r = {0}, root = {0}, value = {0};
	call("fs", "temp_dir", nullptr, 0, &root);
	const char *directory = tstring_cstr(((tstr *)root.val.v_tcompo)->data);
	char a[4096], b[4096], nested[4096], link[4096];
	snprintf(a, sizeof(a), "%s/a", directory);
	snprintf(b, sizeof(b), "%s/b", directory);
	snprintf(nested, sizeof(nested), "%s/sub/child", directory);
	snprintf(link, sizeof(link), "%s/link", directory);
	text(&p[0], nested);
	tobj_set_bool(&p[1], 1);
	call("fs", "mkdir", p, 2, &r);
	call("fs", "mkdir", p, 2, &r);
	assert(mkfifo(a, 0600) == 0);
	text(&p[0], a);
	text(&p[1], b);
	call("fs", "try_copy", p, 2, &r);
	assert(!field_bool(&r, "ok"));
	field_text(&r, "code", "invalid_type");
	assert(unlink(a) == 0);
	text(&p[0], a);
	unsigned char *data = malloc(256);
	for (int i = 0; i < 256; i++)
		data[i] = (unsigned char)i;
	tstdlib_bytes_take(&p[1], data, 256);
	call("io", "write_bytes", p, 2, &r);
	call("io", "read_bytes", p, 1, &r);
	const void *read_data;
	size_t length;
	assert(tstdlib_bytes_data(&r, &read_data, &length) && length == 256 &&
	       !memcmp(read_data, data, 256));
	call("io", "try_append_bytes", p, 2, &r);
	assert(field_bool(&r, "ok"));
	call("io", "try_replace_bytes", p, 2, &r);
	assert(field_bool(&r, "ok"));
	text(&p[1], b);
	call("fs", "copy", p, 2, &r);
	call("fs", "try_copy", p, 2, &r);
	assert(!field_bool(&r, "ok"));
	field_text(&r, "code", "already_exists");
	assert(symlink(a, link) == 0);
	text(&p[0], link);
	call("fs", "lstat", p, 1, &r);
	field_text(&r, "kind", "symlink");
	call("fs", "stat", p, 1, &r);
	field_text(&r, "kind", "file");
	call("fs", "try_copy", p, 2, &r);
	assert(!field_bool(&r, "ok"));
	call("fs", "remove", p, 1, &r);
	assert(access(a, F_OK) == 0);
	assert(symlink("link", link) == 0);
	call("io", "try_read_text", p, 1, &r);
	assert(!field_bool(&r, "ok"));
	field_text(&r, "code", "symlink_loop");
	call("fs", "try_stat", p, 1, &r);
	assert(!field_bool(&r, "ok"));
	field_text(&r, "code", "symlink_loop");
	call("fs", "remove", p, 1, &r);
	text(&p[0], b);
	text(&p[1], link);
	call("fs", "rename", p, 2, &r);
	assert(access(b, F_OK) != 0);
	text(&p[0], directory);
	call("fs", "list_dir", p, 1, &r);
	tlist *names = (tlist *)r.val.v_tcompo;
	assert(tlist_size(names) == 3 && equal(tlist_at(names, 0), "a") &&
	       equal(tlist_at(names, 1), "link") && equal(tlist_at(names, 2), "sub"));
	call("fs", "try_remove", p, 1, &r);
	assert(!field_bool(&r, "ok"));
	field_text(&r, "code", "not_empty");
	text(&p[0], nested);
	call("fs", "remove", p, 1, &r);
	snprintf(nested, sizeof(nested), "%s/sub", directory);
	text(&p[0], nested);
	call("fs", "remove", p, 1, &r);
	text(&p[0], a);
	call("fs", "remove", p, 1, &r);
	text(&p[0], link);
	call("fs", "remove", p, 1, &r);
	text(&p[0], directory);
	call("fs", "temp_file", p, 1, &r);
	tobj_copy(&value, &r);
	call("fs", "stat", &value, 1, &r);
	field_text(&r, "kind", "file");
	call("fs", "remove", &value, 1, &r);
	call("fs", "remove", &root, 1, &r);
	call("fs", "try_exists", &root, 1, &r);
	assert(field_bool(&r, "ok") && !field_bool(&r, "value"));
	call("io", "try_read_bytes", &root, 1, &r);
	assert(!field_bool(&r, "ok"));
	field(&r, "value", &value);
	assert(tstdlib_bytes_data(&value, &read_data, &length) && !length);
	for (int i = 0; i < 2; i++)
		tobj_ddc_ref_clear(&p[i]);
	tobj_ddc_ref_clear(&value);
	tobj_try_clear(&r);
	tobj_try_clear(&root);
}
static void arguments(tobj p[7], const char **argv, size_t count)
{
	for (int i = 0; i < 7; i++)
		tobj_try_clear(&p[i]);
	tlist *list = tlist_new();
	tobj_set_compo(&p[0], (tcompo_v *)list);
	for (size_t i = 0; i < count; i++) {
		tobj value = {0};
		text(&value, argv[i]);
		tobj_vec_push(&list->items, &value);
		tobj_try_clear(&value);
	}
}
static void option(tobj *slot, const char *name, const tobj *value)
{
	tobj key = {0};
	text(&key, name);
	tobj_set_compo(slot, (tcompo_v *)tpair_new(&key, value));
	tobj_try_clear(&key);
}
static int fd_count(void)
{
	int count = 0;
	for (int i = 0; i < 256; i++)
		count += fcntl(i, F_GETFD) >= 0;
	return count;
}
static void check_run(const char *child)
{
	tobj p[7] = {{0}}, r = {0}, value = {0}, out = {0};
	const char *argv[] = {child, "exit", "7"};
	arguments(p, argv, 3);
	call("os", "run", p, 1, &r);
	field_text(&r, "status", "exited");
	assert(field_int(&r, "exit_code") == 7);
	argv[2] = "127";
	arguments(p, argv, 3);
	call("os", "run", p, 1, &r);
	field_text(&r, "status", "exited");
	assert(field_int(&r, "exit_code") == 127);
	const char *bad[] = {"/__tapas_missing_program__"};
	int before = fd_count();
	for (int i = 0; i < 10; i++) {
		arguments(p, bad, 1);
		call("os", "run", p, 1, &r);
		field_text(&r, "status", "spawn_error");
		assert(field_int(&r, "errno") == ENOENT);
	}
	assert(fd_count() == before);
	const char *literal[] = {child, "argument", "a; echo injected"};
	arguments(p, literal, 3);
	call("os", "run", p, 1, &r);
	field_text(&r, "stdout", literal[2]);
	/* No inherited host descriptors, including descriptors without CLOEXEC. */
	int host_fd = open("/dev/null", O_RDONLY);
	assert(host_fd >= 0);
	char fd_name[32];
	snprintf(fd_name, sizeof(fd_name), "%d", host_fd);
	const char *descriptor[] = {child, "descriptor", fd_name};
	arguments(p, descriptor, 3);
	call("os", "run", p, 1, &r);
	assert(field_int(&r, "exit_code") == 0);
	close(host_fd);
	/* Per-call cwd/environment/PATH never modify the calling process. */
	assert(setenv("TAPAS_SYSTEM_TEST", "parent", 1) == 0);
	const char *program = strrchr(child, '/');
	assert(program);
	program++;
	char child_dir[4096];
	snprintf(child_dir, sizeof(child_dir), "%.*s", (int)(program - child - 1), child);
	const char *env_argv[] = {program, "environment"};
	arguments(p, env_argv, 2);
	tdict *env = tdict_new();
	tobj_set_compo(&value, (tcompo_v *)env);
	tobj key = {0}, item = {0}, cwd = {0};
	text(&key, "TAPAS_SYSTEM_TEST");
	text(&item, "child");
	tdict_set(env, &key, &item);
	text(&key, "PATH");
	text(&item, child_dir);
	tdict_set(env, &key, &item);
	tobj_try_clear(&key);
	tobj_try_clear(&item);
	option(&p[1], "env", &value);
	tobj_try_clear(&value);
	tobj_set_bool(&value, 0);
	option(&p[2], "inherit_env", &value);
	call("os", "cwd", nullptr, 0, &cwd);
	option(&p[3], "cwd", &cwd);
	call("os", "run", p, 4, &r);
	char expected[8192];
	snprintf(expected, sizeof(expected), "%s\nchild",
	    tstring_cstr(((tstr *)cwd.val.v_tcompo)->data));
	field_text(&r, "stdout", expected);
	assert(!strcmp(getenv("TAPAS_SYSTEM_TEST"), "parent"));
	tobj_set_nil(&cwd);
	unsetenv("TAPAS_SYSTEM_TEST");
	const char *duplex[] = {child, "duplex"};
	arguments(p, duplex, 2);
	unsigned char *data = malloc(524288);
	for (size_t i = 0; i < 524288; i++)
		data[i] = (unsigned char)i;
	tstdlib_bytes_take(&value, data, 524288);
	option(&p[1], "stdin", &value);
	tobj_try_clear(&value);
	call("os", "run", p, 2, &r);
	field_text(&r, "status", "exited");
	assert(!field_bool(&r, "output_truncated"));
	field(&r, "stdout", &out);
	tstring *s = ((tstr *)out.val.v_tcompo)->data;
	assert(
	    tstring_len(s) == 131072 + 524288 && !memcmp(tstring_cstr(s) + 131072, data, 524288));
	tobj_ddc_ref_clear(&out);
	field(&r, "stderr", &out);
	assert(tstring_len(((tstr *)out.val.v_tcompo)->data) == 131072);
	tobj_ddc_ref_clear(&out);
	const char *limit[] = {child, "limit"};
	arguments(p, limit, 2);
	tobj_set_int(&value, 64);
	option(&p[1], "output_limit", &value);
	call("os", "run", p, 2, &r);
	assert(field_bool(&r, "stdout_truncated") && field_bool(&r, "stderr_truncated"));
	field_text(&r, "status", "exited");
	field(&r, "stdout", &out);
	assert(tstring_len(((tstr *)out.val.v_tcompo)->data) == 64);
	tobj_ddc_ref_clear(&out);
	const char *signal[] = {child, "signal"};
	arguments(p, signal, 2);
	call("os", "run", p, 1, &r);
	field_text(&r, "status", "signaled");
	assert(field_int(&r, "signal") == SIGTERM);
	const char *timeout[] = {child, "ignore"};
	arguments(p, timeout, 2);
	tobj_set_float(&value, 0.05);
	option(&p[1], "timeout", &value);
	call("os", "run", p, 2, &r);
	field_text(&r, "status", "timeout");
	assert(field_int(&r, "signal") == SIGKILL);
	const char *descendant[] = {child, "descendant"};
	arguments(p, descendant, 2);
	option(&p[1], "timeout", &value);
	call("os", "run", p, 2, &r);
	field_text(&r, "status", "timeout");
	/* Cancellation is delivered while the run loop owns the calling thread. */
	char ready[] = "/tmp/tapas-run-cancel-XXXXXX";
	int fd = mkstemp(ready);
	assert(fd >= 0);
	close(fd);
	unlink(ready);
	pid_t notifier = fork();
	assert(notifier >= 0);
	if (!notifier) {
		for (int i = 0; i < 300; i++) {
			if (!access(ready, F_OK)) {
				kill(getppid(), SIGINT);
				_exit(0);
			}
			struct timespec delay = {0, 10000000};
			nanosleep(&delay, nullptr);
		}
		_exit(1);
	}
	const char *cancel[] = {child, "ignore", ready};
	arguments(p, cancel, 3);
	call("os", "run", p, 1, &r);
	field_text(&r, "status", "cancelled");
	assert(field_int(&r, "cancel_signal") == SIGINT);
	int status;
	assert(
	    waitpid(notifier, &status, 0) == notifier && WIFEXITED(status) && !WEXITSTATUS(status));
	unlink(ready);
	/* Signals already blocked by the caller remain pending and retain their mask. */
	sigset_t blocked, original, pending, restored;
	sigemptyset(&blocked);
	sigaddset(&blocked, SIGINT);
	assert(!pthread_sigmask(SIG_BLOCK, &blocked, &original));
	assert(!raise(SIGINT));
	const char *masked[] = {child, "exit", "0"};
	arguments(p, masked, 3);
	call("os", "run", p, 1, &r);
	field_text(&r, "status", "exited");
	assert(!sigpending(&pending) && sigismember(&pending, SIGINT));
	assert(!pthread_sigmask(SIG_SETMASK, nullptr, &restored) && sigismember(&restored, SIGINT));
	int received;
	assert(!sigwait(&blocked, &received) && received == SIGINT);
	assert(!pthread_sigmask(SIG_SETMASK, &original, nullptr));
	/* Child exits without reading: the caller must survive EPIPE. */
	const char *early[] = {child, "exit", "0"};
	arguments(p, early, 3);
	data = malloc(1048576);
	memset(data, 0, 1048576);
	tstdlib_bytes_take(&value, data, 1048576);
	option(&p[1], "stdin", &value);
	tobj_try_clear(&value);
	call("os", "run", p, 2, &r);
	field_text(&r, "status", "exited");
	assert(fd_count() == before);
	for (int i = 0; i < 7; i++)
		tobj_try_clear(&p[i]);
	tobj_try_clear(&r);
	tobj_try_clear(&value);
	tobj_ddc_ref_clear(&out);
}
int main(int argc, char **argv)
{
	assert(argc == 2);
	check_environment();
	check_time();
	check_fs();
	check_run(argv[1]);
	return 0;
}
