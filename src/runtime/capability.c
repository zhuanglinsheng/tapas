#include "tapas/tval.h"


static const tcompo_capabilities *capabilities(tcompo_v *self, const char *operation)
{
	if (!self || !self->vtable || !self->vtable->capabilities)
		twarn(ErrRuntime_RefType, operation, "unsupported capability");
	return self->vtable->capabilities;
}

void tcompo_index(tcompo_v *self, const tobj *arguments,
		  uint_regs argument_count, tobj *result)
{
	const tcompo_capabilities *available = capabilities(self, "idx");
	if (!available->indexable)
		twarn(ErrRuntime_RefType, "idx", "object is not Indexable");
	available->indexable(self, arguments, argument_count, result);
}

void tcompo_index_set(tcompo_v *self, const tobj *arguments,
		      uint_regs argument_count, const tobj *value)
{
	const tcompo_capabilities *available =
		capabilities(self, "index assignment");
	if (!available->index_settable)
		twarn(ErrRuntime_RefType, "index assignment",
		      "object is not IndexSettable");
	available->index_settable(self, arguments, argument_count, value);
}

void tcompo_append(tcompo_v *self, const tobj *value)
{
	const tcompo_capabilities *available = capabilities(self, "append");
	if (!available->appendable)
		twarn(ErrRuntime_RefType, "append", "object is not Appendable");
	available->appendable(self, value);
}

void tcompo_delete(tcompo_v *self, const tobj *key)
{
	const tcompo_capabilities *available = capabilities(self, "delete");
	if (!available->deletable)
		twarn(ErrRuntime_RefType, "delete", "object is not Deletable");
	available->deletable(self, key);
}

int tcompo_contains(tcompo_v *self, const tobj *value)
{
	if (!self || !self->vtable || !self->vtable->capabilities ||
	    !self->vtable->capabilities->contains)
		return 0;
	return self->vtable->capabilities->contains(self, value);
}

int tcompo_next(tcompo_v *self, long *position, tobj *result)
{
	const tcompo_capabilities *available = capabilities(self, "iteration");
	if (!available->iterable)
		twarn(ErrRuntime_RefType, "iteration", "object is not Iterable");
	return available->iterable(self, position, result);
}
