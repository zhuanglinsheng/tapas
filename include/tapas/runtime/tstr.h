#ifndef TAPAS_RUNTIME_TSTR_H
#define TAPAS_RUNTIME_TSTR_H

#include "tapas/tval.h"

#ifdef __cplusplus
extern "C" {
#endif

struct tstr {
	tcompo_v base;
	tstring storage;
	tstring *data;
};

extern tcompo_vtable tstr_vtable;

tstr *tstr_new(const char *s);
tstr *tstr_new_len(const char *s, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_RUNTIME_TSTR_H */
