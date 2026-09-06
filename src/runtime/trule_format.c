#include "tapas/runtime/trule_format.h"
#include "tapas/runtime/trule.h"
#include "tapas/runtime/tdomain.h"
#include "tapas/runtime/tstr.h"
#include "tapas/runtime/tlist.h"
#include "tapas/runtime/tdict.h"
#include "tapas/runtime/tpair.h"
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

typedef struct {
    tstring *out;
    const void *active[24];
    unsigned depth, nodes, type_depth;
    size_t limit;
    int stopped;
} format_context;

static void text(format_context *c, const char *s)
{
    if (c->stopped || !s) return;
    if (tstring_len(c->out) + strlen(s) > c->limit) {
        size_t remaining = c->limit - tstring_len(c->out);
        while (remaining && ((unsigned char)s[remaining] & 0xc0) == 0x80) remaining--;
        tstring_append_len(c->out, s, remaining);
        tstring_append(c->out, "...<truncated>"); c->stopped = 1; return;
    }
    tstring_append(c->out, s);
}
static void number(format_context *c, long n)
{
    char buffer[32]; snprintf(buffer, sizeof(buffer), "%ld", n); text(c, buffer);
}
static void quoted(format_context *c, const char *s)
{
    text(c, "\"");
    for (; s && *s && !c->stopped; s++) {
        switch (*s) {
        case '\\': text(c, "\\\\"); break;
        case '"': text(c, "\\\""); break;
        case '\n': text(c, "\\n"); break;
        case '\r': text(c, "\\r"); break;
        case '\t': text(c, "\\t"); break;
        default: {
            char buffer[8];
            if ((unsigned char)*s < 32) snprintf(buffer, sizeof(buffer), "\\x%02x", (unsigned char)*s);
            else {
                size_t width = 1;
                if ((unsigned char)*s >= 0xc0) {
                    size_t expected = (unsigned char)*s >= 0xf0 ? 4 : (unsigned char)*s >= 0xe0 ? 3 : 2;
                    while (width < expected && s[width] && ((unsigned char)s[width] & 0xc0) == 0x80) width++;
                }
                memcpy(buffer, s, width); buffer[width] = 0; s += width - 1;
            }
            text(c, buffer);
        }
        }
    }
    text(c, "\"");
}
static const char *string_value(const tobj *v)
{
    return v && v->type == tcompo && tobj_compo_type(v) == compo_tstr ?
        tstring_cstr(((tstr *)v->val.v_tcompo)->data) : nullptr;
}
static void value(format_context *, const tobj *, int);
static void object(format_context *, tcompo_v *, int);
static void type(format_context *c, ttypeval *t)
{
    if (!t) { text(c, "AnyType"); return; }
    if (c->type_depth >= 24 || ++c->nodes > 1024) { text(c, "...<limit>"); return; }
    c->type_depth++;
    if (t->kind == ttype_kind_builtin || t->kind == ttype_kind_any) text(c, tbuiltin_name(t->builtin));
    else if (t->kind == ttype_kind_recursive) text(c, "Recursive");
    else if (t->kind == ttype_kind_fields) {
        text(c, "{");
        for (uint_objs i=0; i<ttypeval_field_count(t) && !c->stopped; i++) {
            const tobj *key; ttypeval *field;
            if (!ttypeval_field_at(t,i,&key,&field)) continue;
            if (i) text(c, ", ");
            const char *name=string_value(key); text(c,name ? name : "?");
            if (name && ttypeval_field_optional(t,name)) text(c,"?");
            text(c,": "); type(c,field);
        }
        text(c,"}");
    } else if (t->kind == ttype_kind_enum) {
        text(c,"Enum[");
        for (uint_objs i=0;i<ttypeval_enum_member_count(t) && !c->stopped;i++) {
            if(i) text(c,", "); quoted(c,tstring_cstr(ttypeval_enum_member_at(t,i)));
        }
        text(c,"]");
    } else if (t->kind == ttype_kind_instance_of) {
        text(c,"InstanceOf[");
        if (t->instance_reference && tstring_len(t->instance_reference)) text(c,tstring_cstr(t->instance_reference));
        else value(c,&t->instance_rule,1);
        text(c,"]");
    } else {
        const char *name = t->kind == ttype_kind_function ? "Function" :
            t->kind == ttype_kind_rule ? "Rule" : t->kind == ttype_kind_rule_instance ? "RuleInstance" :
            t->kind == ttype_kind_rule_term ? "Term" : t->kind == ttype_kind_list ? "List" :
            t->kind == ttype_kind_iterator ? "Iterator" : t->kind == ttype_kind_pair ? "Pair" :
            t->kind == ttype_kind_dictionary ? "Dictionary" : t->kind == ttype_kind_union ? "Union" :
            t->kind == ttype_kind_points ? "PointsOf" : t->kind == ttype_kind_range ? "RangeOf" : "Type";
        text(c,name); text(c,"[");
        if (t->kind == ttype_kind_function || t->kind == ttype_kind_rule || t->kind == ttype_kind_rule_instance) {
            if (ttypeval_function_variadic(t)) text(c,"...");
            else for(uint_objs i=0;i<ttypeval_function_parameter_count(t) && !c->stopped;i++) {
                if(i) text(c,", "); type(c,ttypeval_function_parameter_at(t,i));
            }
        } else if(t->kind == ttype_kind_union) {
            for(uint_objs i=0;i<ttypeval_member_count(t) && !c->stopped;i++) {
                if(i) text(c,", "); type(c,ttypeval_member_at(t,i));
            }
        } else if(t->kind == ttype_kind_pair || t->kind == ttype_kind_dictionary) {
            type(c,ttypeval_parameter(t,t->kind == ttype_kind_pair ? "first" : "key"));
            text(c,", "); type(c,ttypeval_parameter(t,t->kind == ttype_kind_pair ? "second" : "value"));
        } else type(c,ttypeval_parameter(t,"item"));
        text(c,"]");
        if(t->kind == ttype_kind_function) { text(c," -> "); type(c,ttypeval_function_result(t)); }
    }
    c->type_depth--;
}
static void named(format_context *c, const char *kind, const char *name)
{
    text(c,kind);
    if (name && *name) { text(c," "); text(c,name); }
}
static void function_signature(format_context *c, const char *name, const tfunction_metadata *m)
{
    named(c,"Function",name); text(c,"[");
    if (!m) text(c,"...");
    else if (tfunction_metadata_variadic(m)) text(c,"...");
    else for(uint32_t i=0;i<tfunction_metadata_count(m) && !c->stopped;i++) {
        if(i) text(c,", "); type(c,tfunction_metadata_type(m,i));
    }
    text(c,"] -> "); type(c,tfunction_metadata_return_type(m));
}
static void sequence(format_context *c, const tobj_vec *v)
{
    for (uint_objs i = 0; i < v->len && !c->stopped; i++) {
        if (i) text(c, ", "); value(c, &v->data[i], 1);
    }
}
static void rule_signature(format_context *c, const char *kind, const trule_ir *ir, uint64_t identity)
{
    const char *name = ir ? tstring_cstr(ir->display_name) : nullptr;
    named(c,kind,name);
    if ((!name || !*name) && identity) {
        char label[32]; snprintf(label,sizeof(label)," #%" PRIu64,identity); text(c,label);
    }
    text(c,"[");
    if (ir) for(uint_objs i=0;i<ir->parameters.len && !c->stopped;i++) {
        if(i) text(c,", ");
        type(c,((trule_term *)ir->parameters.data[i].val.v_tcompo)->type);
    }
    text(c,"]");
}
static void term(format_context *c, const trule_term *t, int full)
{
    if (!full) {
        const char *name = (t->kind == trule_term_parameter || t->kind == trule_term_capture)
            ? string_value(&t->payload) : nullptr;
        text(c, trule_term_kind_name(t->kind));
        if (name && *name) { text(c, "("); text(c, name); text(c, ")"); }
        else { text(c, "#"); number(c, (long)t->id); }
        return;
    }
    const char *op = string_value(&t->payload);
    if (t->kind == trule_term_parameter) {
        text(c, op ? op : "?"); return;
    }
    if (t->kind == trule_term_capture) {
        text(c, "capture("); value(c, &t->payload, 1); text(c, ": "); type(c, t->type); text(c, ")"); return;
    }
    if (t->kind == trule_term_constant) { value(c, &t->payload, 1); return; }
    if (t->kind == trule_term_not && t->arguments.len == 1) {
        text(c, "not ("); value(c, &t->arguments.data[0], 1); text(c, ")"); return;
    }
    const char *binary = t->kind == trule_term_and ? "and" : t->kind == trule_term_or ? "or" : t->kind == trule_term_in ? "in" : nullptr;
    if (!binary && t->kind == trule_term_intrinsic && op &&
        (!strcmp(op,"+") || !strcmp(op,"-") || !strcmp(op,"*") || !strcmp(op,"/") ||
         !strcmp(op,"==") || !strcmp(op,"!=") || !strcmp(op,"<") || !strcmp(op,">") ||
         !strcmp(op,"<=") || !strcmp(op,">="))) binary = op;
    if (binary && t->arguments.len == 2) {
        text(c, "("); value(c, &t->arguments.data[0], 1); text(c, " "); text(c, binary);
        text(c, " "); value(c, &t->arguments.data[1], 1); text(c, ")"); return;
    }
    if (t->kind == trule_term_call) {
        text(c, "("); value(c, &t->payload, 1); text(c, ")("); sequence(c, &t->arguments); text(c, ")"); return;
    }
    /* Preserve unknown/extension structure explicitly instead of guessing syntax. */
    text(c, trule_term_kind_name(t->kind)); text(c, "(payload="); value(c, &t->payload, 1);
    if (tstring_len(t->provider)) { text(c, ", provider="); quoted(c, tstring_cstr(t->provider)); }
    if (tstring_len(t->provider_kind)) { text(c, ", kind="); quoted(c, tstring_cstr(t->provider_kind)); }
    text(c, ", arguments=["); sequence(c, &t->arguments); text(c, "])");
}
static void item(format_context *c, const trule_item *i, int full)
{
    const char *name = i->kind == trule_item_condition ? "Condition" : i->kind == trule_item_requirement ? "Requirement" : "Implication";
    text(c, name);
    if (!full) return;
    text(c, "[");
    if (i->kind == trule_item_requirement) {
        object(c, (tcompo_v *)i->rule, 1); text(c, ", arguments=["); sequence(c, &i->arguments); text(c, "]");
    } else {
        object(c, (tcompo_v *)i->term, 1);
        if (i->kind == trule_item_implication) { text(c, " implies ["); sequence(c, &i->arguments); text(c, "]"); }
    }
    if (tstring_len(i->description)) { text(c, ", description="); quoted(c, tstring_cstr(i->description)); }
    text(c, "]");
}
static void declarations(format_context *c, const tobj_vec *terms)
{
    text(c, "[");
    for (uint_objs i=0; i<terms->len && !c->stopped; i++) {
        if (i) text(c, ", ");
        const trule_term *t = (trule_term *)terms->data[i].val.v_tcompo;
        const char *name = string_value(&t->payload);
        if (name && *name) text(c, name);
        else { text(c, "#"); number(c, (long)t->id); }
        text(c, ": "); type(c, t->type);
    }
    text(c, "]");
}
static void ir_structure(format_context *c, const trule_ir *ir)
{
    named(c, "RuleIR", tstring_cstr(ir->display_name));
    text(c, " {\n  parameters: "); declarations(c, &ir->parameters);
    text(c, ",\n  captures: "); declarations(c, &ir->captures);
    text(c, ",\n  conditions: [");
    for (uint_objs i=0; i<ir->items.len && !c->stopped; i++) {
        text(c, i ? ",\n    " : "\n    ");
        const trule_item *entry = (trule_item *)ir->items.data[i].val.v_tcompo;
        if (entry->kind == trule_item_condition) object(c, (tcompo_v *)entry->term, 1);
        else value(c, &ir->items.data[i], 1);
    }
    text(c, ir->items.len ? "\n  ]\n}" : "]\n}");
}
static const char *builtin_name(trule_builtin_kind kind)
{
    static const char *names[] = {"assert", "rules::check", "rules::inspect", "rules::parameters", "rules::items",
        "rules::terms", "rules::origin", "rules::semantic_hash", "rules::content_hash", "rules::serialize",
        "rules::deserialize", "evaluators::eval", "evaluators::compile", "evaluators::binding", "evaluators::capture",
        "evaluators::value", "evaluators::requirement", "solve::hold"};
    return (unsigned)kind < sizeof(names)/sizeof(names[0]) ? names[kind] : "unknown";
}
typedef struct { format_context *format; int count; } dictionary_context;
static void pair(const tobj *key, const tobj *val, void *opaque)
{
    dictionary_context *d = opaque;
    if (d->format->stopped) return;
    if (d->count++) text(d->format, ", ");
    value(d->format, key, 1); text(d->format, ": "); value(d->format, val, 1);
}
static void object(format_context *c, tcompo_v *v, int full)
{
    if (!v) { text(c, "nil"); return; }
    if (c->stopped) return;
    for (unsigned i=0; i<c->depth; i++) if (c->active[i]==v) { text(c, "<cycle>"); return; }
    if (c->depth == 24 || ++c->nodes > 1024) { text(c, "...<limit>"); return; }
    c->active[c->depth++] = v;
    switch (v->vtable->get_compo_type_code()) {
    case compo_tfunc: {
        tfunc *f=(tfunc *)v;
        function_signature(c,tfunction_metadata_display_name(f->metadata),f->metadata);
    } break;
    case compo_trule: rule_signature(c,"Rule",((trule *)v)->ir,((trule *)v)->identity); break;
    case compo_trule_instance: {
        trule_instance *i=(trule_instance *)v;
        trule *r=(trule *)i->rule.val.v_tcompo;
        text(c, "RuleInstance[");
        const char *name = r->ir ? tstring_cstr(r->ir->display_name) : nullptr;
        if (name && *name) text(c, name);
        else {
            char label[32]; snprintf(label, sizeof(label), "#%" PRIu64, r->identity);
            text(c, label);
        }
        if (i->arguments.len) text(c, "; ");
        for (uint_objs j=0; j<i->arguments.len && !c->stopped; j++) {
            if (j) text(c, ", ");
            trule_term *p = r->ir && j<r->ir->parameters.len ? (trule_term *)r->ir->parameters.data[j].val.v_tcompo : nullptr;
            if (p && string_value(&p->payload)) text(c, string_value(&p->payload));
            else { text(c, "p"); number(c, j); }
            text(c, "="); value(c, &i->arguments.data[j], 1);
        }
        text(c, "]");
    } break;
    case compo_trule_builtin: {
        trule_builtin *b=(trule_builtin *)v;
        function_signature(c,builtin_name(b->kind),b->metadata);
    } break;
    case compo_trule_term: term(c, (trule_term *)v, full); break;
    case compo_trule_item: item(c, (trule_item *)v, full); break;
    case compo_trule_ir: ir_structure(c,(trule_ir *)v); break;
    case compo_tpoints: case compo_trange: {
        tdomain *d=(tdomain *)v;
        text(c, d->range ? "RangeOf[" : "PointsOf["); type(c, d->item_type); text(c, "][");
        if (d->range) { number(c,d->start); text(c,", "); number(c,d->end); }
        else if (full) sequence(c, &d->values);
        else { number(c,d->values.len); text(c," values"); }
        text(c,"]");
    } break;
    case compo_tstr: quoted(c, tstring_cstr(((tstr *)v)->data)); break;
    case compo_tlist: text(c,"["); sequence(c,&((tlist *)v)->items); text(c,"]"); break;
    case compo_tdict: {
        dictionary_context d={.format=c}; text(c,"{"); thashtbl_each(((tdict *)v)->items,pair,&d); text(c,"}");
    } break;
    case compo_tpair: text(c,"("); value(c,&((tpair *)v)->first,1); text(c,": "); value(c,&((tpair *)v)->second,1); text(c,")"); break;
    case compo_ttypeval: type(c,(ttypeval *)v); break;
    default: { tstring *s=v->vtable->tostring_abbr(v); text(c,tstring_cstr(s)); tstring_free(s); } break;
    }
    c->depth--;
}
static void value(format_context *c, const tobj *v, int full)
{
    if (v->type == tcompo) { object(c,v->val.v_tcompo,full); return; }
    tstring *s=tobj_tostring_full(v); text(c,tstring_cstr(s)); tstring_free(s);
}
static tstring *format(void *self, int full)
{
    format_context c={.out=tstring_new_empty(), .limit=full ? 16384 : 256};
    tcompo_v *v = self;
    if (full && v && v->vtable->get_compo_type_code() == compo_trule_term) {
        trule_term *t = self;
        const char *name = (t->kind == trule_term_parameter || t->kind == trule_term_capture) ? string_value(&t->payload) : nullptr;
        text(&c, "Term "); text(&c, trule_term_kind_name(t->kind)); text(&c, "[");
        if (name) { text(&c, name); text(&c, ": "); type(&c, t->type); }
        else { object(&c, v, 1); text(&c, ": "); type(&c, t->type); }
        text(&c, "]");
    } else {
        if (full && v && v->vtable->get_compo_type_code() == compo_trule_item) text(&c, "Item ");
        object(&c,v,full);
    }
    return c.out;
}
tstring *trule_format_abbr(void *self)
{
    tstring *out=tstring_new_empty();
    tstring_append_fmt(out,"<%p>",self);
    return out;
}
tstring *trule_format_full(void *self) { return format(self,1); }
