#include "tapas/runtime/tsolve.h"
#include "tapas/runtime/tdict.h"
#include "tapas/runtime/tdomain.h"
#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tstr.h"
#include "tapas/runtime/tcfn.h"
#include "tapas/textension.h"
#include "../modules.h"
#include "solve_backend.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <spawn.h>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>
#include <stdint.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
extern char **environ;
#endif

#define SOLVE_INT_BOUND 1000000000000L
#define SOLVE_RULE_LIMIT 128

typedef struct {
    tstring *out;
    trule *rules[SOLVE_RULE_LIMIT];
    unsigned count;
    unsigned depth;
    int fields;
} export_context;

static void json_string(tstring *out, const char *text)
{
    tstring_append_c(out, '"');
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p == '"' || *p == '\\') {
            tstring_append_c(out, '\\');
            tstring_append_c(out, *p);
        }
        else if (*p < 32)
            tstring_append_fmt(out, "\\u%04x", *p);
        else
            tstring_append_c(out, *p);
    }
    tstring_append_c(out, '"');
}

static void export_type(export_context *ctx, const ttypeval *type)
{
    const char *kind = type->kind == ttype_kind_fields ? "record" :
        type->kind == ttype_kind_enum ? "enum" :
        ttypeval_equal(type, ttypeval_builtin(tbuiltin_int)) ? "int" :
        ttypeval_equal(type, ttypeval_builtin(tbuiltin_bool)) ? "bool" :
        ttypeval_equal(type, ttypeval_builtin(tbuiltin_string)) ? "string" : "unsupported";
    tstring_append(ctx->out, "{\"kind\":");
    json_string(ctx->out, kind);
    if (type->kind == ttype_kind_fields && ctx->depth < 32) {
        ctx->depth++;
        tstring_append(ctx->out, ",\"fields\":[");
        for (uint_objs i = 0; i < ttypeval_field_count(type); i++) {
            const tobj *name; ttypeval *field_type;
            if (!ttypeval_field_at(type, i, &name, &field_type)) abort();
            if (i) tstring_append_c(ctx->out, ',');
            const char *text = tstring_cstr(((tstr *)name->val.v_tcompo)->data);
            tstring_append_c(ctx->out, '['); json_string(ctx->out, text);
            tstring_append_c(ctx->out, ','); export_type(ctx, field_type);
            tstring_append_fmt(ctx->out, ",%s]", ttypeval_field_optional(type, text) ? "true" : "false");
        }
        tstring_append_c(ctx->out, ']'); ctx->depth--;
    }
    if (type->kind == ttype_kind_enum) {
        tstring_append(ctx->out, ",\"members\":[");
        for (uint_objs i = 0; i < type->enum_member_count; i++) {
            if (i)
                tstring_append_c(ctx->out, ',');
            json_string(ctx->out, tstring_cstr(type->enum_members[i]));
        }
        tstring_append_c(ctx->out, ']');
    }
    tstring_append_c(ctx->out, '}');
}

static int register_rule(export_context *ctx, trule *rule)
{
    for (unsigned i = 0; i < ctx->count; i++)
        if (ctx->rules[i] == rule)
            return (int)i;
    if (ctx->count == SOLVE_RULE_LIMIT)
        return -1;
    ctx->rules[ctx->count] = rule;
    return (int)ctx->count++;
}

static void export_value(export_context *ctx, const tobj *value);

static void export_field(const tobj *key, const tobj *value, void *opaque)
{
    export_context *ctx = opaque;
    if (ctx->fields++)
        tstring_append_c(ctx->out, ',');
    tstring_append_c(ctx->out, '[');
    export_value(ctx, key);
    tstring_append_c(ctx->out, ',');
    export_value(ctx, value);
    tstring_append_c(ctx->out, ']');
}

static void export_value(export_context *ctx, const tobj *value)
{
    if (++ctx->depth > 32) {
        tstring_append(ctx->out, "{\"unsupported\":\"cyclic or deeply nested value\"}");
        ctx->depth--;
        return;
    }
    switch (value->type) {
    case tnil:
        tstring_append(ctx->out, "null");
        break;
    case tint:
        tstring_append_fmt(ctx->out, "%ld", value->val.v_tint);
        break;
    case tbool:
        tstring_append(ctx->out, value->val.v_tbool ? "true" : "false");
        break;
    case tcompo:
        switch (tobj_compo_type(value)) {
        case compo_tstr:
            json_string(ctx->out, tstring_cstr(((tstr *)value->val.v_tcompo)->data));
            break;
        case compo_trule: {
            int index = register_rule(ctx, (trule *)value->val.v_tcompo);
            if (index < 0)
                tstring_append(ctx->out, "{\"unsupported\":\"too many Rules\"}");
            else
                tstring_append_fmt(ctx->out, "{\"rule\":%d}", index);
        } break;
        case compo_trule_instance: {
            trule_instance *instance = (trule_instance *)value->val.v_tcompo;
            int index = register_rule(ctx, (trule *)instance->rule.val.v_tcompo);
            if (index < 0) {
                tstring_append(ctx->out, "{\"unsupported\":\"too many Rules\"}");
                break;
            }
            tstring_append_fmt(ctx->out, "{\"instance\":%d,\"args\":[", index);
            for (uint_objs i = 0; i < instance->arguments.len; i++) {
                if (i)
                    tstring_append_c(ctx->out, ',');
                export_value(ctx, &instance->arguments.data[i]);
            }
            tstring_append(ctx->out, "]}");
        } break;
        case compo_trule_term:
            tstring_append_fmt(ctx->out, "{\"term\":%llu}", (unsigned long long)((trule_term *)value->val.v_tcompo)->id);
            break;
        case compo_ttypeval:
            tstring_append(ctx->out, "{\"type\":"); export_type(ctx, (ttypeval *)value->val.v_tcompo);
            tstring_append_c(ctx->out, '}');
            break;
        case compo_trange: {
            tdomain *domain = (tdomain *)value->val.v_tcompo;
            tstring_append_fmt(ctx->out, "{\"range\":[%ld,%ld]}", domain->start, domain->end);
        } break;
        case compo_tpoints: {
            tdomain *domain = (tdomain *)value->val.v_tcompo;
            tstring_append(ctx->out, "{\"points\":[");
            for (uint_objs i = 0; i < domain->values.len; i++) {
                if (i)
                    tstring_append_c(ctx->out, ',');
                export_value(ctx, &domain->values.data[i]);
            }
            tstring_append(ctx->out, "]}");
        } break;
        case compo_tlist: {
            tlist *list = (tlist *)value->val.v_tcompo;
            tstring_append(ctx->out, "{\"list\":[");
            for (uint_objs i = 0; i < tlist_size(list); i++) {
                if (i)
                    tstring_append_c(ctx->out, ',');
                export_value(ctx, tlist_at(list, i));
            }
            tstring_append(ctx->out, "]}");
        } break;
        case compo_tlib: {
            tdict *exposed = tlib_get_exposed((tlib *)value->val.v_tcompo);
            if (!exposed) {
                tstring_append(ctx->out, "{\"unsupported\":\"module has no exported data\"}");
                break;
            }
            tobj contents = {.type = tcompo, .val.v_tcompo = (tcompo_v *)exposed};
            export_value(ctx, &contents);
        } break;
        case compo_tdict: {
            tdict *dict = (tdict *)value->val.v_tcompo;
            int saved = ctx->fields; ctx->fields = 0;
            tstring_append_fmt(ctx->out, "{\"identity\":\"%p\",\"dict\":[", (void *)dict);
            thashtbl_each(dict->items, export_field, ctx);
            tstring_append(ctx->out, "]}"); ctx->fields = saved;
        } break;
        case compo_cppfunc: {
            tcppgenf *function = (tcppgenf *)value->val.v_tcompo;
            const char *name = nullptr;
            /* Whitelist by function identity, not an untrusted display name. */
            for (uint32_t i = 0; i < tstdlib_rules_module.symbol_count; i++) {
                const textension_symbol *symbol = &tstdlib_rules_module.symbols[i];
                if (symbol->function == function->f &&
                    (!strcmp(symbol->name, "range") || !strcmp(symbol->name, "points")))
                    name = symbol->name;
            }
            if (name)
                tstring_append_fmt(ctx->out, "{\"native\":\"rules::%s\"}", name);
            else
                tstring_append(ctx->out, "{\"unsupported\":\"native function\"}");
        } break;
        default:
            tstring_append(ctx->out, "{\"unsupported\":\"runtime value\"}");
            break;
        }
        break;
    default:
        tstring_append(ctx->out, "{\"unsupported\":\"non-integer value\"}");
        break;
    }
    ctx->depth--;
}

static void export_terms(export_context *ctx, trule *rule)
{
    unsigned locals = 0;
    if (rule->checker.type == tcompo && tobj_compo_type(&rule->checker) == compo_tfunc) {
        const tcompo_env *env = &((tfunc *)rule->checker.val.v_tcompo)->env;
        locals = env->base.objs.capacity + env->tmpmax - env->nparams;
    }
    tstring_append_fmt(ctx->out, "{\"local_count\":%u,\"parameters\":[", locals);
    for (uint_objs i = 0; i < rule->ir->parameters.len; i++) {
        if (i) tstring_append_c(ctx->out, ',');
        tstring_append_fmt(ctx->out, "%llu", (unsigned long long)((trule_term *)rule->ir->parameters.data[i].val.v_tcompo)->id);
    }
    tstring_append(ctx->out, "],\"terms\":{");
    for (uint_objs i = 0; i < rule->ir->terms.len; i++) {
        trule_term *term = (trule_term *)rule->ir->terms.data[i].val.v_tcompo;
        if (i) tstring_append_c(ctx->out, ',');
        tstring_append_fmt(ctx->out, "\"%llu\":{\"kind\":", (unsigned long long)term->id);
        json_string(ctx->out, trule_term_kind_name(term->kind));
        tstring_append(ctx->out, ",\"type\":"); export_type(ctx, term->type);
        tstring_append(ctx->out, ",\"payload\":"); export_value(ctx, &term->payload);
        tstring_append(ctx->out, ",\"arguments\":[");
        for (uint_objs j = 0; j < term->arguments.len; j++) {
            if (j) tstring_append_c(ctx->out, ',');
            tstring_append_fmt(ctx->out, "%llu", (unsigned long long)((trule_term *)term->arguments.data[j].val.v_tcompo)->id);
        }
        tstring_append_c(ctx->out, ']');
        if (term->kind == trule_term_capture) {
            tobj captured; tobj_set_nil(&captured);
            tstring_append(ctx->out, ",\"value\":");
            if (trule_read_capture(rule, term, &captured)) export_value(ctx, &captured);
            else tstring_append(ctx->out, "{\"unsupported\":\"unavailable capture\"}");
            tobj_ddc_ref_clear(&captured);
        }
        tstring_append_c(ctx->out, '}');
    }
    tstring_append(ctx->out, "},\"items\":[");
    for (uint_objs i = 0; i < rule->ir->items.len; i++) {
        trule_item *item = (trule_item *)rule->ir->items.data[i].val.v_tcompo;
        if (i) tstring_append_c(ctx->out, ',');
        tstring_append(ctx->out, "{\"kind\":");
        json_string(ctx->out, item->kind == trule_item_condition ? "Condition" : item->kind == trule_item_requirement ? "Requirement" : "Implication");
        trule_term *term = item->kind == trule_item_requirement ? item->rule : item->term;
        tstring_append_fmt(ctx->out, ",\"term\":%llu,\"arguments\":[", (unsigned long long)term->id);
        for (uint_objs j = 0; j < item->arguments.len; j++) {
            if (j) tstring_append_c(ctx->out, ',');
            tstring_append_fmt(ctx->out, "%llu", (unsigned long long)((trule_term *)item->arguments.data[j].val.v_tcompo)->id);
        }
        tstring_append(ctx->out, "]}");
    }
    tstring_append(ctx->out, "]}");
}

static void field(tdict *dict, const char *name, const tobj *value)
{
    tobj key; tobj_set_nil(&key); tobj_set_compo(&key, (tcompo_v *)tstr_new(name));
    tdict_set(dict, &key, value); tobj_try_clear(&key);
}

static void string_field(tdict *dict, const char *name, const char *text)
{
    tobj value; tobj_set_nil(&value); tobj_set_compo(&value, (tcompo_v *)tstr_new(text));
    field(dict, name, &value); tobj_try_clear(&value);
}

static void result_set(tobj *result, const char *status, const char *scope, const char *reason)
{
    tdict *dict = tdict_new();
    string_field(dict, "status", status); string_field(dict, "scope", scope); string_field(dict, "reason", reason);
    tobj value; tobj_set_nil(&value); field(dict, "witness", &value);
    tobj_set_compo(&value, (tcompo_v *)tlist_new()); field(dict, "conflicts", &value); tobj_try_clear(&value);
    tobj_set_compo(&value, (tcompo_v *)tdict_new()); field(dict, "bindings", &value); tobj_try_clear(&value);
    tobj_set_int(&value, -SOLVE_INT_BOUND); field(dict, "integer_min", &value);
    tobj_set_int(&value, SOLVE_INT_BOUND); field(dict, "integer_max", &value);
    tobj_set_compo(result, (tcompo_v *)dict);
}

void tsolve_reject_witness(tobj *result, const char *reason)
{
    tdict *dict = (tdict *)result->val.v_tcompo;
    string_field(dict, "status", "error"); string_field(dict, "reason", reason);
    tobj nil; tobj_set_nil(&nil); field(dict, "witness", &nil);
}

#if !defined(_WIN32)
struct tsolve_worker { int fd; pid_t pid; char *python; };

void tsolve_worker_free(tsolve_worker *worker)
{
    if (!worker) return;
    close(worker->fd);
    kill(worker->pid, SIGKILL);
    while (waitpid(worker->pid, nullptr, 0) < 0 && errno == EINTR) {}
    free(worker->python); free(worker);
}

static int64_t milliseconds(void)
{
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

/* One deadline covers both transfer directions; never block on a full socket. */
static int transfer(int fd, void *data, size_t size, int writing, int64_t deadline)
{
    char *bytes = data;
    while (size) {
        int64_t remaining = deadline - milliseconds();
        if (remaining <= 0) return 0;
        struct pollfd event = {.fd = fd, .events = writing ? POLLOUT : POLLIN};
        int ready = poll(&event, 1, (int)remaining);
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) return ready;
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags = MSG_NOSIGNAL;
#endif
        ssize_t n = writing ? send(fd, bytes, size, flags) : recv(fd, bytes, size, 0);
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (n <= 0) return -1;
        bytes += n; size -= (size_t)n;
    }
    return 1;
}

static tsolve_worker *start_worker(const char *python)
{
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets)) return nullptr;
    /* Keep descriptors above stdio even when the host has closed stdin/stdout. */
    for (int i = 0; i < 2; i++) {
        int fd = fcntl(sockets[i], F_DUPFD_CLOEXEC, 3);
        if (fd < 0) { close(sockets[0]); close(sockets[1]); return nullptr; }
        close(sockets[i]); sockets[i] = fd;
    }
#ifdef SO_NOSIGPIPE
    int enabled = 1;
    setsockopt(sockets[0], SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, sockets[1], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, sockets[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, sockets[0]);
    posix_spawn_file_actions_addclose(&actions, sockets[1]);
    char *argv[] = {(char *)python, "-B", "-u", "-c", (char *)tapas_solve_backend, nullptr};
    pid_t pid;
    int error = posix_spawnp(&pid, python, &actions, nullptr, argv, environ);
    posix_spawn_file_actions_destroy(&actions); close(sockets[1]);
    if (error) { close(sockets[0]); return nullptr; }
    tsolve_worker *worker = calloc(1, sizeof(*worker));
    if (!worker) abort();
    worker->fd = sockets[0]; worker->pid = pid; worker->python = strdup(python);
    if (!worker->python) abort();
    if (fcntl(worker->fd, F_SETFL, O_NONBLOCK) < 0) { tsolve_worker_free(worker); return nullptr; }
    return worker;
}

static int read_line(char **stream, char *line, size_t capacity)
{
    char *end = strchr(*stream, '\n');
    if (!end || (size_t)(end - *stream) >= capacity) return 0;
    size_t n = (size_t)(end - *stream);
    memcpy(line, *stream, n); line[n] = 0; *stream = end + 1; return 1;
}

static int hex_digit(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

static int decode_argument(const char *line, tobj *argument)
{
    if (line[0] == 'b' && (line[1] == '0' || line[1] == '1') && !line[2]) {
        tobj_set_bool(argument, line[1] == '1'); return 1;
    }
    if (line[0] == 'i') {
        char *end; errno = 0; long value = strtol(line + 1, &end, 10);
        if (errno || end == line + 1 || *end) return 0;
        tobj_set_int(argument, value); return 1;
    }
    if (line[0] == 's') {
        size_t length = strlen(line + 1);
        if (length % 2) return 0;
        char *text = calloc(length / 2 + 1, 1);
        if (!text) abort();
        for (size_t i = 0; i < length / 2; i++) {
            int a = hex_digit(line[1 + i * 2]), b = hex_digit(line[2 + i * 2]);
            if (a < 0 || b < 0 || !(a * 16 + b)) { free(text); return 0; }
            text[i] = (char)(a * 16 + b);
        }
        tobj_set_compo(argument, (tcompo_v *)tstr_new(text)); free(text); return 1;
    }
    return 0;
}
#endif

#if defined(_WIN32)
void tsolve_worker_free(tsolve_worker *worker) { (void)worker; }
#endif

static void hold_configured(tsolve_worker **worker, const tobj *input, tobj *result, long integer_min, long integer_max)
{
    result_set(result, "error", "configured", "invalid solve input");
    if (input->type != tcompo || (tobj_compo_type(input) != compo_trule && tobj_compo_type(input) != compo_trule_instance)) return;
#if defined(_WIN32)
    result_set(result, "error", "configured", "the first solve process adapter requires POSIX");
#else
    trule_instance *instance = tobj_compo_type(input) == compo_trule_instance ? (trule_instance *)input->val.v_tcompo : nullptr;
    trule *rule = instance ? (trule *)instance->rule.val.v_tcompo : (trule *)input->val.v_tcompo;
    export_context ctx = {.out = tstring_new_empty()}; register_rule(&ctx, rule);
    tstring_append_fmt(ctx.out, "{\"version\":3,\"integer_min\":%ld,\"integer_max\":%ld,\"bindings\":", integer_min, integer_max);
    if (instance) {
        tstring_append_c(ctx.out, '[');
        for (uint_objs i = 0; i < instance->arguments.len; i++) {
            if (i) tstring_append_c(ctx.out, ','); export_value(&ctx, &instance->arguments.data[i]);
        }
        tstring_append_c(ctx.out, ']');
    } else tstring_append(ctx.out, "null");
    tstring_append(ctx.out, ",\"rules\":[");
    for (unsigned i = 0; i < ctx.count; i++) {
        if (i) tstring_append_c(ctx.out, ','); export_terms(&ctx, ctx.rules[i]);
        if (tstring_len(ctx.out) > 16 * 1024 * 1024) break;
    }
    tstring_append(ctx.out, "]}");
    if (tstring_len(ctx.out) > 16 * 1024 * 1024) {
        tstring_free(ctx.out); result_set(result, "unsupported", "configured", "prepared model exceeds size limit"); return;
    }
    const char *python = getenv("TAPAS_SOLVE_PYTHON");
    if (!python || !*python) python = "python3";
    if (*worker && strcmp((*worker)->python, python)) {
        tsolve_worker_free(*worker); *worker = nullptr;
    }
    if (!*worker) *worker = start_worker(python);
    if (!*worker) {
        tstring_free(ctx.out);
        result_set(result, "error", "configured", "cannot start Python backend (set TAPAS_SOLVE_PYTHON)"); return;
    }
    size_t size = tstring_len(ctx.out);
    unsigned char frame[4] = {size >> 24, size >> 16, size >> 8, size};
    int64_t deadline = milliseconds() + 15000;
    int ok = transfer((*worker)->fd, frame, 4, 1, deadline);
    if (ok == 1) ok = transfer((*worker)->fd, (void *)tstring_cstr(ctx.out), size, 1, deadline);
    tstring_free(ctx.out);
    if (ok == 1) ok = transfer((*worker)->fd, frame, 4, 0, deadline);
    size = ((uint32_t)frame[0] << 24) | ((uint32_t)frame[1] << 16) | ((uint32_t)frame[2] << 8) | frame[3];
    if (ok == 1 && (!size || size > 4 * 1024 * 1024)) ok = -1;
    char *buffer = ok == 1 ? calloc(size + 1, 1) : nullptr;
    if (ok == 1 && !buffer) abort();
    if (ok == 1) ok = transfer((*worker)->fd, buffer, size, 0, deadline);
    if (ok == 1 && memchr(buffer, 0, size)) ok = -1;
    if (ok != 1) {
        free(buffer); tsolve_worker_free(*worker); *worker = nullptr;
        result_set(result, ok == 0 ? "unknown" : "error", "configured",
            ok == 0 ? "solver process timed out" : "Python backend communication failed"); return;
    }
    char *response = buffer;
    char header[32], code[32], scope[32], reason[4096], line[8192];
    int valid = response && read_line(&response, header, sizeof(header)) && !strcmp(header, "TAPAS_SOLVE_3") &&
        read_line(&response, code, sizeof(code)) && read_line(&response, scope, sizeof(scope)) &&
        read_line(&response, reason, sizeof(reason)) && read_line(&response, line, sizeof(line));
    if (valid) valid = (!strcmp(code, "sat") || !strcmp(code, "unsat") || !strcmp(code, "unknown") || !strcmp(code, "unsupported") || !strcmp(code, "error")) &&
        !strcmp(scope, "configured");
    char *end = nullptr;
    long count = valid ? strtol(line, &end, 10) : -1;
    valid = valid && end != line && !*end && count >= 0 && count <= UINT8_MAX;
    int sat = valid && !strcmp(code, "sat");
    if (valid) valid = count == (sat && !instance ? rule->ir->parameters.len : 0);
    tobj arguments[UINT8_MAX];
    for (unsigned i = 0; i < UINT8_MAX; i++) tobj_set_nil(&arguments[i]);
    for (long i = 0; valid && i < count; i++) {
        valid = read_line(&response, line, sizeof(line)) && decode_argument(line, &arguments[i]);
        if (valid && arguments[i].type == tint) valid = arguments[i].val.v_tint >= integer_min && arguments[i].val.v_tint <= integer_max;
        if (valid) valid = ttypeval_matches(&arguments[i], ((trule_term *)rule->ir->parameters.data[i].val.v_tcompo)->type);
    }
    if (valid && sat && instance) {
        for (uint_objs i = 0; i < instance->arguments.len; i++) {
            tobj *argument = &instance->arguments.data[i];
            if (argument->type == tint && (argument->val.v_tint < integer_min || argument->val.v_tint > integer_max)) valid = 0;
        }
    }
    tobj conflicts; tobj_set_nil(&conflicts);
    tobj_set_compo(&conflicts, (tcompo_v *)tlist_new());
    if (valid) valid = read_line(&response, line, sizeof(line));
    end = nullptr; errno = 0;
    long conflict_count = valid ? strtol(line, &end, 10) : -1;
    valid = valid && !errno && end != line && !*end && conflict_count >= 0 && conflict_count <= 256;
    if (valid && strcmp(code, "unsat")) valid = conflict_count == 0;
    for (long i = 0; valid && i < conflict_count; i++) {
        valid = read_line(&response, line, sizeof(line));
        end = nullptr; errno = 0;
        long index = valid ? strtol(line, &end, 10) : -1;
        valid = valid && !errno && end != line && !*end && index >= 0 && index < ctx.count;
        tobj condition; tobj_set_nil(&condition);
        if (valid) valid = read_line(&response, line, sizeof(line)) && line[0] == 's' && decode_argument(line, &condition);
        if (valid) {
            tobj entry, origin; tobj_set_nil(&entry); tobj_set_nil(&origin);
            tdict *dict = tdict_new(); tobj_set_compo(&entry, (tcompo_v *)dict);
            tobj_set_compo(&origin, (tcompo_v *)ctx.rules[index]);
            field(dict, "rule", &origin); field(dict, "condition", &condition);
            tobj_vec_push(&((tlist *)conflicts.val.v_tcompo)->items, &entry);
            tobj_try_clear(&entry);
        }
        tobj_try_clear(&condition);
    }
    if (valid) valid = !*response;
    free(buffer);
    if (!valid) { tsolve_worker_free(*worker); *worker = nullptr; }
    if (valid) {
        result_set(result, code, scope, reason);
        field((tdict *)result->val.v_tcompo, "conflicts", &conflicts);
        if (sat) {
            tobj witness; tobj_set_nil(&witness);
            if (instance) { tobj_copy(&witness, input); }
            else tobj_set_compo(&witness, (tcompo_v *)trule_bind(rule, arguments, (uint_regs)count));
            field((tdict *)result->val.v_tcompo, "witness", &witness);
            if (instance) tobj_ddc_ref_clear(&witness); else tobj_try_clear(&witness);
        }
    } else result_set(result, "error", "configured", "invalid solver response");
    tobj_try_clear(&conflicts);
    for (unsigned i = 0; i < UINT8_MAX; i++) tobj_try_clear(&arguments[i]);
#endif
}

static int read_integer_setting(const char *name, long fallback, long *value)
{
    const char *text = getenv(name);
    *value = fallback;
    if (!text)
        return 1;
    const char *digits = text;
    if (*digits == '+' || *digits == '-')
        digits++;
    if (!*digits)
        return 0;
    for (const char *p = digits; *p; p++) if (*p < '0' || *p > '9')
        return 0;
    char *end;
    errno = 0;
    long parsed = strtol(text, &end, 10);
    if (errno || *end || parsed == LONG_MIN || parsed == LONG_MAX)
        return 0;
    *value = parsed;
    return 1;
}

/* Specialize only unconditional Parameter == Constant constraints. Keep every
 * original condition, including contradictory equalities, in the solver query.
 * This is query preparation, not execution of a checker or user function. */
static void collect_fixed(trule *rule, trule *root, const tobj **fixed, unsigned depth, unsigned *budget)
{
    if (depth >= 64 || !*budget) return;
    (*budget)--;
    for (uint_objs i = 0; i < rule->ir->items.len; i++) {
        trule_item *item = (trule_item *)rule->ir->items.data[i].val.v_tcompo;
        if (item->kind == trule_item_requirement && item->rule->kind == trule_term_constant &&
            item->rule->payload.type == tcompo && tobj_compo_type(&item->rule->payload) == compo_trule) {
            trule *child = (trule *)item->rule->payload.val.v_tcompo;
            int same = child->ir->parameters.len == root->ir->parameters.len && item->arguments.len == root->ir->parameters.len;
            for (uint_objs j = 0; same && j < item->arguments.len; j++)
                same = item->arguments.data[j].val.v_tcompo == root->ir->parameters.data[j].val.v_tcompo &&
                    child->ir->parameters.data[j].val.v_tcompo == root->ir->parameters.data[j].val.v_tcompo;
            if (same) collect_fixed(child, root, fixed, depth + 1, budget);
        }
        if (item->kind != trule_item_condition) continue;
        trule_term *term = item->term;
        if (term->kind != trule_term_intrinsic || term->arguments.len != 2 ||
            term->payload.type != tcompo || tobj_compo_type(&term->payload) != compo_tstr ||
            strcmp(tstring_cstr(((tstr *)term->payload.val.v_tcompo)->data), "==")) continue;
        trule_term *a = (trule_term *)term->arguments.data[0].val.v_tcompo;
        trule_term *b = (trule_term *)term->arguments.data[1].val.v_tcompo;
        if (a->kind == trule_term_constant) { trule_term *swap = a; a = b; b = swap; }
        if (a->kind != trule_term_parameter || b->kind != trule_term_constant) continue;
        for (uint_objs j = 0; j < root->ir->parameters.len; j++)
            if (!fixed[j] && root->ir->parameters.data[j].val.v_tcompo == (tcompo_v *)a &&
                ttypeval_matches(&b->payload, a->type)) fixed[j] = &b->payload;
    }
}

static void prepare_fixed_instance(const tobj *input, tobj *prepared)
{
    tobj_set_nil(prepared);
    if (input->type != tcompo || tobj_compo_type(input) != compo_trule) return;
    trule *rule = (trule *)input->val.v_tcompo;
    if (!rule->ir->parameters.len || rule->ir->parameters.len > UINT8_MAX) return;
    const tobj *fixed[UINT8_MAX] = {0};
    unsigned budget = 4096;
    collect_fixed(rule, rule, fixed, 0, &budget);
    tobj arguments[UINT8_MAX];
    for (uint_objs i = 0; i < rule->ir->parameters.len; i++) {
        if (!fixed[i]) return;
        arguments[i] = *fixed[i];
    }
    tobj_set_compo(prepared, (tcompo_v *)trule_bind(rule, arguments, (uint_regs)rule->ir->parameters.len));
}

void tsolve_hold(tsolve_worker **worker, const tobj *input, tobj *result)
{
    long minimum = -SOLVE_INT_BOUND, maximum = SOLVE_INT_BOUND;
    if (!read_integer_setting("TAPAS_SOLVE_INT_MIN", -SOLVE_INT_BOUND, &minimum) ||
        !read_integer_setting("TAPAS_SOLVE_INT_MAX", SOLVE_INT_BOUND, &maximum) || minimum > maximum) {
        result_set(result, "error", "configured", "invalid TAPAS_SOLVE_INT_MIN/MAX: expected ordered integer bounds within backend limits");
        return;
    }
    tobj prepared; prepare_fixed_instance(input, &prepared);
    const tobj *query_input = prepared.type == tnil ? input : &prepared;
    hold_configured(worker, query_input, result, minimum, maximum);
    tobj value; tobj_set_nil(&value);
    tobj_set_int(&value, minimum); field((tdict *)result->val.v_tcompo, "integer_min", &value);
    tobj_set_int(&value, maximum); field((tdict *)result->val.v_tcompo, "integer_max", &value);
    if (input->type == tcompo && tobj_compo_type(input) == compo_trule_instance) {
        trule_instance *instance = (trule_instance *)input->val.v_tcompo;
        trule *rule = (trule *)instance->rule.val.v_tcompo;
        tobj bindings; tobj_set_nil(&bindings);
        tdict *dict = tdict_new(); tobj_set_compo(&bindings, (tcompo_v *)dict);
        for (uint_objs i = 0; i < instance->arguments.len; i++) {
            trule_term *parameter = (trule_term *)rule->ir->parameters.data[i].val.v_tcompo;
            if (parameter->payload.type == tcompo && tobj_compo_type(&parameter->payload) == compo_tstr)
                tdict_set(dict, &parameter->payload, &instance->arguments.data[i]);
            else {
                char name[32]; snprintf(name, sizeof(name), "p%u", (unsigned)i);
                field(dict, name, &instance->arguments.data[i]);
            }
        }
        field((tdict *)result->val.v_tcompo, "bindings", &bindings); tobj_try_clear(&bindings);
    }
    tobj_try_clear(&prepared);
}

ttypeval *tsolve_result_type(void)
{
    tstring *rule_name = tstring_new("rule"), *condition_name = tstring_new("condition");
    ttype_field row_fields[] = {
        {.name = rule_name, .type = ttypeval_builtin(tbuiltin_rule)},
        {.name = condition_name, .type = ttypeval_builtin(tbuiltin_string)}
    };
    ttypeval *row = ttypeval_new_fields(row_fields, 2);
    ttypeval *list = ttypeval_new_list(row);
    const char *names[] = {"status", "scope", "reason", "witness", "integer_min", "integer_max", "conflicts", "bindings"};
    ttype_field fields[8]; tstring *strings[8];
    for (unsigned i = 0; i < 8; i++) {
        strings[i] = tstring_new(names[i]);
        fields[i] = (ttype_field){.name = strings[i], .type = i == 7 ? ttypeval_builtin(tbuiltin_dictionary) : i == 6 ? list : ttypeval_builtin(i < 3 ? tbuiltin_string : i == 3 ? tbuiltin_any : tbuiltin_int)};
    }
    ttypeval *type = ttypeval_new_fields(fields, 8);
    for (unsigned i = 0; i < 8; i++) tstring_free(strings[i]);
    /* The enclosing types own list and row through their definition tables. */
    tstring_free(rule_name); tstring_free(condition_name);
    return type;
}

static void create_hold(tobj *result)
{
    trule_builtin *hold = trule_builtin_new(trule_builtin_hold);
    ttypeval *members[] = {ttypeval_builtin(tbuiltin_rule), ttypeval_builtin(tbuiltin_rule_instance)};
    ttypeval *parameter = ttypeval_new_union(members, 2);
    ttypeval *signature = ttypeval_retain(ttypeval_new_function(&parameter, 1, tsolve_result_type(), 0));
    hold->metadata = tfunction_metadata_from_type("solve::hold\x1e" "rule", signature);
    ttypeval_release(signature);
    tobj_set_compo(result, (tcompo_v *)hold);
}

static void create_result(tobj *result) {
    tobj_set_compo(result, (tcompo_v *)tsolve_result_type());
}

static const textension_symbol symbols[] = {
    {
        .name = "hold",
        .type = "Function[Rule | RuleInstance] -> solve::HoldResult",
        .detail = "solve::hold(rule: Rule | RuleInstance) -> solve::HoldResult",
        .kind = textension_value,
        .value_factory = create_hold
    },
    {
        .name = "HoldResult",
        .type = "{status: String, scope: String, reason: String, witness: Unknown, integer_min: Int, integer_max: Int, conflicts: List[{rule: Rule, condition: String}], bindings: Dictionary}",
        .detail = "Feasibility status, scope, witness and diagnostics",
        .kind = textension_type,
        .value_factory = create_result
    }
};

const textension_module tstdlib_solve_module = {
    .scope = textension_package,
    .name = "solve",
    .detail = "Rule feasibility queries using CP-SAT",
    .symbols = symbols,
    .symbol_count = sizeof(symbols) / sizeof(symbols[0])
};
