#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "tapas/textension.h"
#include "tapas/objects/tlist.h"
#include "tapas/objects/tpair.h"
#include "tapas/dsa/thashtbl.h"
#include "../system.h"
#include "../text.h"
#include "../bytes/buffer.h"
#include <errno.h>
#include <dirent.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef enum { pipe_stdin, pipe_stdout, pipe_stderr, pipe_exec_error, pipe_count } pipe_role;

enum { pipe_read, pipe_write };

enum { stream_chunk_size = 16384, poll_interval_ms = 20, default_output_limit = 1024 * 1024 };
static const double default_timeout_seconds = 30.0;
static const double termination_grace_seconds = 0.1;
static const double capture_grace_seconds = 0.3;

extern char **environ;

typedef struct {
	const void *input;
	size_t input_length;
	const char *cwd;
	tdict *environment;
	int inherit_environment;
	double timeout;
	size_t output_limit;
} run_options;

static const char *argument_cstring(const tobj *value, const char *what, int nonempty)
{
	tstr *string = tstdlib_string_arg(value, "os::run");
	const char *text = tstring_cstr(string->data);
	size_t n = tstring_len(string->data);
	if ((nonempty && !n) || memchr(text, 0, n))
		twarn(ErrRuntime_ParamsType, "os::run", what);
	return text;
}

static void validate_env(const tobj *key, const tobj *value, void *context)
{
	(void)context;
	const char *name =
	    argument_cstring(key, "environment name must be nonempty and contain no NUL", 1);
	if (strchr(name, '='))
		twarn(ErrRuntime_ParamsType, "os::run", "environment name contains '='");
	argument_cstring(value, "environment value contains NUL", 0);
}

typedef enum {
	option_stdin = 1u << 0,
	option_cwd = 1u << 1,
	option_env = 1u << 2,
	option_inherit_env = 1u << 3,
	option_timeout = 1u << 4,
	option_output_limit = 1u << 5
} option_flag;

static run_options parse_options(tobj *params, uint_regs count)
{
	run_options config = {.input = "",
	    .inherit_environment = 1,
	    .timeout = default_timeout_seconds,
	    .output_limit = default_output_limit};
	unsigned seen = 0;
	for (unsigned i = 1; i < count; i++) {
		if (params[i].type != tcompo || tobj_compo_type(&params[i]) != compo_tpair)
			twarn(ErrRuntime_ParamsType, "os::run", "options must be named");
		tpair *pair = (tpair *)params[i].val.v_tcompo;
		const char *name = argument_cstring(&pair->first, "invalid option name", 1);
		const tobj *v = &pair->second;
		unsigned bit = 0;
		if (!strcmp(name, "stdin")) {
			bit = option_stdin;
			if (v->type == tcompo && tobj_compo_type(v) == compo_tstr) {
				tstr *s = (tstr *)v->val.v_tcompo;
				config.input = tstring_cstr(s->data);
				config.input_length = tstring_len(s->data);
			} else if (!tstdlib_bytes_data(v, &config.input, &config.input_length))
				twarn(ErrRuntime_ParamsType, "os::run",
				    "stdin must be String or bytes::Buffer");
		} else if (!strcmp(name, "cwd")) {
			bit = option_cwd;
			config.cwd =
			    argument_cstring(v, "cwd must be nonempty and contain no NUL", 1);
		} else if (!strcmp(name, "env")) {
			bit = option_env;
			if (v->type != tcompo || tobj_compo_type(v) != compo_tdict)
				twarn(ErrRuntime_ParamsType, "os::run", "env must be Dictionary");
			config.environment = (tdict *)v->val.v_tcompo;
			thashtbl_each(config.environment->items, validate_env, nullptr);
		} else if (!strcmp(name, "inherit_env")) {
			bit = option_inherit_env;
			if (v->type != tbool)
				twarn(ErrRuntime_ParamsType, "os::run", "inherit_env must be Bool");
			config.inherit_environment = v->val.v_tbool;
		} else if (!strcmp(name, "timeout")) {
			bit = option_timeout;
			if (v->type != tint && v->type != tfloat)
				twarn(ErrRuntime_ParamsType, "os::run",
				    "timeout must be numeric seconds");
			config.timeout = v->type == tint ? (double)v->val.v_tint : v->val.v_tfloat;
			if (!isfinite(config.timeout) || config.timeout <= 0 ||
			    config.timeout > 1e9)
				twarn(ErrRuntime_ParamsType, "os::run",
				    "timeout must be finite, positive and at most 1e9 seconds");
		} else if (!strcmp(name, "output_limit")) {
			bit = option_output_limit;
			if (v->type != tint || v->val.v_tint < 0)
				twarn(ErrRuntime_ParamsType, "os::run",
				    "output_limit must be nonnegative Int");
			config.output_limit = (size_t)v->val.v_tint;
		} else
			twarn(ErrRuntime_ParamsType, "os::run", "unknown option");
		if (seen & bit)
			twarn(ErrRuntime_ParamsType, "os::run", "duplicate option");
		seen |= bit;
	}
	return config;
}

static void free_strings(char **strings)
{
	if (strings) {
		for (size_t i = 0; strings[i]; i++)
			free(strings[i]);
		free(strings);
	}
}

typedef struct {
	char **values;
	size_t count;
} environment_builder;

static void override_env(const tobj *key, const tobj *value, void *data)
{
	environment_builder *env = data;
	const char *name = argument_cstring(key, "invalid environment name", 1),
		   *text = argument_cstring(value, "invalid environment value", 0);
	size_t n = strlen(name), m = strlen(text);
	char *item = malloc(n + m + 2);
	if (!item)
		twarn(ErrRuntime_Other, "os::run", "out of memory");
	memcpy(item, name, n);
	item[n] = '=';
	memcpy(item + n + 1, text, m + 1);
	for (size_t i = 0; i < env->count; i++)
		if (!strncmp(env->values[i], name, n) && env->values[i][n] == '=') {
			free(env->values[i]);
			env->values[i] = item;
			return;
		}
	env->values[env->count++] = item;
}

static char **make_environment(const run_options *config)
{
	size_t inherited = 0;
	if (config->inherit_environment)
		while (environ[inherited])
			inherited++;
	size_t extra = config->environment ? thashtbl_len(config->environment->items) : 0;
	environment_builder env = {.values = calloc(inherited + extra + 1, sizeof(char *))};
	if (!env.values)
		twarn(ErrRuntime_Other, "os::run", "out of memory");
	for (; env.count < inherited; env.count++) {
		env.values[env.count] = strdup(environ[env.count]);
		if (!env.values[env.count]) {
			free_strings(env.values);
			twarn(ErrRuntime_Other, "os::run", "out of memory");
		}
	}
	if (config->environment)
		thashtbl_each(config->environment->items, override_env, &env);
	return env.values;
}

static char **executable_candidates(const char *program, char **env)
{
	const char *path = "/usr/bin:/bin";
	for (size_t i = 0; env[i]; i++)
		if (!strncmp(env[i], "PATH=", 5)) {
			path = env[i] + 5;
			break;
		}
	size_t count = 1;
	if (!strchr(program, '/'))
		for (const char *p = path; *p; p++)
			count += *p == ':';
	char **paths = calloc(count + 1, sizeof(char *));
	if (!paths)
		twarn(ErrRuntime_Other, "os::run", "out of memory");
	if (strchr(program, '/')) {
		paths[0] = strdup(program);
		if (!paths[0]) {
			free(paths);
			twarn(ErrRuntime_Other, "os::run", "out of memory");
		}
		return paths;
	}
	const char *start = path;
	for (size_t i = 0; i < count; i++) {
		const char *end = strchr(start, ':');
		size_t n = end ? (size_t)(end - start) : strlen(start), m = strlen(program);
		paths[i] = malloc(n + m + 2);
		if (!paths[i]) {
			free_strings(paths);
			twarn(ErrRuntime_Other, "os::run", "out of memory");
		}
		memcpy(paths[i], start, n);
		if (n)
			paths[i][n++] = '/';
		memcpy(paths[i] + n, program, m + 1);
		if (end)
			start = end + 1;
	}
	return paths;
}

static void close_fd(int *fd)
{
	if (*fd >= 0) {
		close(*fd);
		*fd = -1;
	}
}

static int pipe_cloexec(int p[2])
{
	if (pipe(p))
		return 0;
	for (int i = 0; i < 2; i++) {
		if (p[i] < 3) {
			int copy = fcntl(p[i], F_DUPFD, 3);
			if (copy < 0)
				goto fail;
			close(p[i]);
			p[i] = copy;
		}
		if (fcntl(p[i], F_SETFD, FD_CLOEXEC) < 0)
			goto fail;
	}
	return 1;
fail: {
	int saved = errno;
	close_fd(&p[0]);
	close_fd(&p[1]);
	errno = saved;
	return 0;
}
}

static int nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL);
	return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

static void child_fail(int fd, int error)
{
	ssize_t n;
	do {
		n = write(fd, &error, sizeof(error));
	} while (n < 0 && errno == EINTR);
	_exit(127);
}

static int *descriptor_snapshot(size_t *count, int *fallback)
{
	*count = 0;
	*fallback = 0;
	DIR *directory = opendir("/dev/fd");
	if (!directory) {
		struct rlimit limit;
		*fallback = getrlimit(RLIMIT_NOFILE, &limit) == 0 && limit.rlim_cur < INT_MAX
				? (int)limit.rlim_cur
				: 65536;
		return nullptr;
	}
	size_t capacity = 32;
	int *fds = malloc(capacity * sizeof(int));
	if (!fds) {
		closedir(directory);
		*fallback = -1;
		errno = ENOMEM;
		return nullptr;
	}
	struct dirent *entry;
	for (;;) {
		errno = 0;
		entry = readdir(directory);
		if (!entry) {
			if (errno) {
				int saved = errno;
				free(fds);
				closedir(directory);
				*fallback = -1;
				errno = saved;
				return nullptr;
			}
			break;
		}
		char *end;
		long number = strtol(entry->d_name, &end, 10);
		if (*end || number < 3 || number > INT_MAX)
			continue;
		if (*count == capacity) {
			capacity *= 2;
			void *grown = realloc(fds, capacity * sizeof(int));
			if (!grown) {
				free(fds);
				closedir(directory);
				*fallback = -1;
				errno = ENOMEM;
				return nullptr;
			}
			fds = grown;
		}
		fds[(*count)++] = (int)number;
	}
	closedir(directory);
	return fds;
}

static void execute_child(int pipes[pipe_count][2], const run_options *config, char **argv,
    char **env, char **paths, const sigset_t *mask, const int *fds, size_t fd_count, int fallback)
{
	/* After fork, use only async-signal-safe operations until exec or _exit. */
	int error_fd = pipes[pipe_exec_error][pipe_write];
	if (setpgid(0, 0))
		child_fail(error_fd, errno);
	if (dup2(pipes[pipe_stdin][pipe_read], STDIN_FILENO) < 0 ||
	    dup2(pipes[pipe_stdout][pipe_write], STDOUT_FILENO) < 0 ||
	    dup2(pipes[pipe_stderr][pipe_write], STDERR_FILENO) < 0)
		child_fail(error_fd, errno);
	if (fds) {
		for (size_t i = 0; i < fd_count; i++)
			if (fds[i] != error_fd)
				close(fds[i]);
	} else {
		for (int fd = 3; fd < fallback; fd++)
			if (fd != error_fd)
				close(fd);
	}
	if (sigprocmask(SIG_SETMASK, mask, nullptr))
		child_fail(error_fd, errno);
	if (config->cwd && chdir(config->cwd))
		child_fail(error_fd, errno);
	int denied = 0, last = ENOENT;
	for (size_t i = 0; paths[i]; i++) {
		execve(paths[i], argv, env);
		last = errno;
		if (last == EACCES)
			denied = 1;
		else if (last != ENOENT && last != ENOTDIR)
			child_fail(error_fd, last);
	}
	child_fail(error_fd, denied ? EACCES : last);
}

typedef enum {
	run_pending,
	run_exited,
	run_signaled,
	run_timeout,
	run_cancelled,
	run_spawn_error,
	run_io_error
} run_status;

/* All descriptors and capture buffers belong to this invocation. */
typedef struct {
	int pipes[pipe_count][2];
	pid_t pid;
	int reaped;
	int wait_status;
	run_status status;
	tstdlib_error error;

	tstring *output[2];
	int truncated[2];
	size_t input_written;
	unsigned char exec_error[sizeof(int)];
	size_t exec_error_bytes;

	int *inherited_fds;
	size_t inherited_fd_count;
	int descriptor_limit;
	sigset_t original_mask;
	int signals_blocked;
	int prior_sigpipe;
	int cancel_signal;

	double started_at;
	double termination_at;
	int clock_started;
	int kill_sent;
} run_state;

static void fail_run(run_state *state, run_status status, int number, const char *operation)
{
	/* Preserve the first failure; cleanup must not hide its cause. */
	if (state->status != run_pending)
		return;
	state->status = status;
	tstdlib_syserr(&state->error, number, operation);
}

static void initialize_run(run_state *state)
{
	memset(state, 0, sizeof(*state));
	state->pid = -1;
	state->termination_at = -1;
	for (int i = 0; i < pipe_count; i++) {
		state->pipes[i][pipe_read] = -1;
		state->pipes[i][pipe_write] = -1;
	}
	state->output[0] = tstring_new_empty();
	state->output[1] = tstring_new_empty();
}

static int start_run(
    run_state *state, const run_options *config, char **argv, char **env, char **paths)
{
	if (!state->output[0] || !state->output[1]) {
		fail_run(state, run_io_error, ENOMEM, "capture allocation");
		return 0;
	}
	if (!tstdlib_monotonic(&state->started_at)) {
		fail_run(state, run_io_error, errno, "monotonic clock");
		return 0;
	}
	state->clock_started = 1;
	for (int i = 0; i < pipe_count; i++) {
		if (!pipe_cloexec(state->pipes[i])) {
			fail_run(state, run_spawn_error, errno, "pipe");
			return 0;
		}
	}
	if (!nonblock(state->pipes[pipe_stdin][pipe_write]) ||
	    !nonblock(state->pipes[pipe_stdout][pipe_read]) ||
	    !nonblock(state->pipes[pipe_stderr][pipe_read]) ||
	    !nonblock(state->pipes[pipe_exec_error][pipe_read])) {
		fail_run(state, run_spawn_error, errno, "nonblocking pipe");
		return 0;
	}
	state->inherited_fds =
	    descriptor_snapshot(&state->inherited_fd_count, &state->descriptor_limit);
	if (state->descriptor_limit < 0) {
		fail_run(state, run_spawn_error, errno, "descriptor snapshot");
		return 0;
	}

	sigset_t blocked, pending;
	sigemptyset(&blocked);
	sigaddset(&blocked, SIGPIPE);
	sigaddset(&blocked, SIGINT);
	sigaddset(&blocked, SIGTERM);
	int error = pthread_sigmask(SIG_BLOCK, &blocked, &state->original_mask);
	if (error) {
		fail_run(state, run_spawn_error, error, "signal mask");
		return 0;
	}
	state->signals_blocked = 1;
	sigpending(&pending);
	state->prior_sigpipe = sigismember(&pending, SIGPIPE);

	state->pid = fork();
	if (state->pid < 0) {
		fail_run(state, run_spawn_error, errno, "fork");
		return 0;
	}
	if (!state->pid)
		execute_child(state->pipes, config, argv, env, paths, &state->original_mask,
		    state->inherited_fds, state->inherited_fd_count, state->descriptor_limit);

	/* The child also establishes the group before exec; this closes the race. */
	setpgid(state->pid, state->pid);
	close_fd(&state->pipes[pipe_stdin][pipe_read]);
	close_fd(&state->pipes[pipe_stdout][pipe_write]);
	close_fd(&state->pipes[pipe_stderr][pipe_write]);
	close_fd(&state->pipes[pipe_exec_error][pipe_write]);
	if (!config->input_length)
		close_fd(&state->pipes[pipe_stdin][pipe_write]);
	return 1;
}

static void collect_cancellation(run_state *state)
{
	sigset_t pending;
	sigpending(&pending);
	const int signals[] = {SIGINT, SIGTERM};
	for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
		int number = signals[i];
		if (sigismember(&state->original_mask, number) || !sigismember(&pending, number))
			continue;
		sigset_t one;
		sigemptyset(&one);
		sigaddset(&one, number);
		int received;
		sigwait(&one, &received);
		if (state->status == run_pending) {
			state->status = run_cancelled;
			state->cancel_signal = number;
		}
	}
}

static void advance_termination(run_state *state, const run_options *config)
{
	double now;
	if (!tstdlib_monotonic(&now)) {
		fail_run(state, run_io_error, errno, "monotonic clock");
		/* A broken clock must not leave termination waiting on a frozen deadline. */
		now = state->termination_at < 0 ? state->started_at + config->timeout
						: state->termination_at + 1.0;
	}
	collect_cancellation(state);
	if (state->status == run_pending && now - state->started_at >= config->timeout)
		state->status = run_timeout;
	if (state->status == run_pending)
		return;

	if (state->termination_at < 0) {
		state->termination_at = now;
		if (state->status != run_spawn_error)
			kill(-state->pid, SIGTERM);
		close_fd(&state->pipes[pipe_stdin][pipe_write]);
	}
	if (now - state->termination_at >= termination_grace_seconds && !state->kill_sent &&
	    state->status != run_spawn_error) {
		kill(-state->pid, SIGKILL);
		state->kill_sent = 1;
	}
	if (now - state->termination_at >= capture_grace_seconds) {
		for (int i = pipe_stdout; i <= pipe_stderr; i++) {
			if (state->pipes[i][pipe_read] >= 0)
				state->truncated[i - pipe_stdout] = 1;
			close_fd(&state->pipes[i][pipe_read]);
		}
		close_fd(&state->pipes[pipe_exec_error][pipe_read]);
	}
}

static void reap_child(run_state *state)
{
	if (state->reaped)
		return;
	pid_t waited = waitpid(state->pid, &state->wait_status, WNOHANG);
	if (waited == state->pid) {
		state->reaped = 1;
		close_fd(&state->pipes[pipe_stdin][pipe_write]);
	} else if (waited < 0 && errno != EINTR) {
		state->reaped = 1;
		fail_run(state, run_io_error, errno, "waitpid");
	}
}

static void write_input(run_state *state, const run_options *config)
{
	size_t remaining = config->input_length - state->input_written;
	size_t chunk = remaining > stream_chunk_size ? stream_chunk_size : remaining;
	int *fd = &state->pipes[pipe_stdin][pipe_write];
	ssize_t count = write(*fd, (const char *)config->input + state->input_written, chunk);
	if (count > 0) {
		state->input_written += (size_t)count;
		if (state->input_written == config->input_length)
			close_fd(fd);
	} else if (count < 0 && errno != EAGAIN && errno != EINTR) {
		/* The child may legitimately exit without consuming all input. */
		if (errno != EPIPE)
			fail_run(state, run_io_error, errno, "stdin pipe");
		close_fd(fd);
	}
}

static void capture_output(
    run_state *state, pipe_role role, const char *data, size_t count, size_t limit)
{
	int index = role - pipe_stdout;
	tstring *output = state->output[index];
	size_t previous = tstring_len(output);
	size_t available = limit - previous;
	if (count > available) {
		count = available;
		state->truncated[index] = 1;
	}
	if (count)
		tstring_append_len(output, data, count);
	if (tstring_len(output) - previous != count)
		fail_run(state, run_io_error, ENOMEM, "capture allocation");
}

static void capture_exec_error(run_state *state, const char *data, size_t count)
{
	size_t remaining = sizeof(int) - state->exec_error_bytes;
	if (count > remaining)
		count = remaining;
	memcpy(state->exec_error + state->exec_error_bytes, data, count);
	state->exec_error_bytes += count;
	if (state->exec_error_bytes == sizeof(int)) {
		int number;
		memcpy(&number, state->exec_error, sizeof(number));
		fail_run(state, run_spawn_error, number, "exec/child setup");
	}
}

static void read_pipe(run_state *state, pipe_role role, const run_options *config)
{
	char buffer[stream_chunk_size];
	int *fd = &state->pipes[role][pipe_read];
	ssize_t count = read(*fd, buffer, sizeof(buffer));
	if (!count) {
		close_fd(fd);
	} else if (count < 0) {
		if (errno != EAGAIN && errno != EINTR) {
			fail_run(state, run_io_error, errno, "capture pipe");
			close_fd(fd);
		}
	} else if (role == pipe_exec_error) {
		capture_exec_error(state, buffer, (size_t)count);
	} else {
		capture_output(state, role, buffer, (size_t)count, config->output_limit);
	}
}

static void wait_for_run(run_state *state, const run_options *config)
{
	for (;;) {
		advance_termination(state, config);
		reap_child(state);
		if (state->reaped && state->pipes[pipe_stdout][pipe_read] < 0 &&
		    state->pipes[pipe_stderr][pipe_read] < 0 &&
		    state->pipes[pipe_exec_error][pipe_read] < 0)
			return;

		/* Service all three streams together: sequential blocking I/O can deadlock. */
		struct pollfd fds[pipe_count] = {{state->pipes[pipe_stdin][pipe_write], POLLOUT, 0},
		    {state->pipes[pipe_stdout][pipe_read], POLLIN, 0},
		    {state->pipes[pipe_stderr][pipe_read], POLLIN, 0},
		    {state->pipes[pipe_exec_error][pipe_read], POLLIN, 0}};
		if (poll(fds, pipe_count, poll_interval_ms) < 0) {
			if (errno != EINTR)
				fail_run(state, run_io_error, errno, "poll");
			continue;
		}
		if (fds[pipe_stdin].fd >= 0 && fds[pipe_stdin].revents)
			write_input(state, config);
		for (int i = pipe_stdout; i < pipe_count; i++) {
			if (fds[i].fd >= 0 && fds[i].revents)
				read_pipe(state, (pipe_role)i, config);
		}
	}
}

static void cleanup_run(run_state *state)
{
	for (int i = 0; i < pipe_count; i++) {
		close_fd(&state->pipes[i][pipe_read]);
		close_fd(&state->pipes[i][pipe_write]);
	}
	if (state->pid > 0 && !state->reaped) {
		kill(-state->pid, SIGKILL);
		while (waitpid(state->pid, &state->wait_status, 0) < 0 && errno == EINTR) {
		}
	}
	if (state->signals_blocked) {
		/* Preserve caller-owned pending signals; consume only our unmasked SIGPIPE. */
		if (!state->prior_sigpipe && !sigismember(&state->original_mask, SIGPIPE)) {
			sigset_t pending;
			sigpending(&pending);
			if (sigismember(&pending, SIGPIPE)) {
				sigset_t one;
				sigemptyset(&one);
				sigaddset(&one, SIGPIPE);
				int received;
				sigwait(&one, &received);
			}
		}
		pthread_sigmask(SIG_SETMASK, &state->original_mask, nullptr);
	}
	free(state->inherited_fds);
}

static const char *status_name(run_status status)
{
	switch (status) {
	case run_exited:
		return "exited";
	case run_signaled:
		return "signaled";
	case run_timeout:
		return "timeout";
	case run_cancelled:
		return "cancelled";
	case run_spawn_error:
		return "spawn_error";
	case run_io_error:
		return "io_error";
	case run_pending:
		break;
	}
	return "io_error";
}

static void make_run_result(run_state *state, tobj *result)
{
	if (state->status == run_pending)
		state->status = WIFSIGNALED(state->wait_status) ? run_signaled : run_exited;
	double finished_at = state->started_at;
	tstdlib_monotonic(&finished_at);
	double elapsed = state->clock_started && finished_at >= state->started_at
			     ? finished_at - state->started_at
			     : 0;
	long exit_code = state->status == run_exited && WIFEXITED(state->wait_status)
			     ? WEXITSTATUS(state->wait_status)
			     : -1;
	long signal = state->pid > 0 && state->reaped && WIFSIGNALED(state->wait_status)
			  ? WTERMSIG(state->wait_status)
			  : 0;

	tdict *record = tdict_new_sized(12);
	tobj_set_compo(result, (tcompo_v *)record);
	tstdlib_field_string(record, "status", status_name(state->status));
	tstdlib_field_int(record, "exit_code", exit_code);
	tstdlib_field_int(record, "signal", signal);
	tstdlib_field_int(record, "cancel_signal", state->cancel_signal);
	tstdlib_field_int(record, "errno", state->error.system_errno);
	tstdlib_field_string(record, "error", state->error.message);
	tstdlib_field_float(record, "elapsed", elapsed);
	tstdlib_field_bool(record, "output_truncated", state->truncated[0] || state->truncated[1]);
	tstdlib_field_bool(record, "stdout_truncated", state->truncated[0]);
	tstdlib_field_bool(record, "stderr_truncated", state->truncated[1]);

	const char *names[] = {"stdout", "stderr"};
	for (int i = 0; i < 2; i++) {
		tstring *output = state->output[i];
		tobj value = {0};
		tstdlib_string_result(&value, output ? tstring_cstr(output) : "",
		    output ? tstring_len(output) : 0, "os::run");
		tstdlib_field(record, names[i], &value);
		tobj_try_clear(&value);
	}
}

void tstdlib_os_run(tobj *params, uint_regs count, tobj *result)
{
	if (!count || params[0].type != tcompo || tobj_compo_type(&params[0]) != compo_tlist)
		twarn(ErrRuntime_ParamsType, "os::run", "nonempty List[String] required");
	tlist *list = (tlist *)params[0].val.v_tcompo;
	size_t argc = tlist_size(list);
	if (!argc)
		twarn(ErrRuntime_ParamsType, "os::run", "argument list must not be empty");
	for (size_t i = 0; i < argc; i++)
		argument_cstring(tlist_at(list, (uint_count)i),
		    "arguments must contain no NUL; executable must be nonempty", i == 0);
	run_options config = parse_options(params, count);

	/* Validate all borrowed input before acquiring process resources. */
	char **argv = calloc(argc + 1, sizeof(char *));
	if (!argv)
		twarn(ErrRuntime_Other, "os::run", "out of memory");
	for (size_t i = 0; i < argc; i++) {
		argv[i] =
		    strdup(argument_cstring(tlist_at(list, (uint_count)i), "invalid argument", 0));
		if (!argv[i]) {
			free_strings(argv);
			twarn(ErrRuntime_Other, "os::run", "out of memory");
		}
	}
	char **env = make_environment(&config);
	char **paths = executable_candidates(argv[0], env);

	run_state state;
	initialize_run(&state);
	if (start_run(&state, &config, argv, env, paths))
		wait_for_run(&state, &config);
	cleanup_run(&state);
	free_strings(argv);
	free_strings(env);
	free_strings(paths);
	make_run_result(&state, result);
	tstring_free(state.output[0]);
	tstring_free(state.output[1]);
}
