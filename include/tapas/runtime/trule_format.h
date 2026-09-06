#ifndef TAPAS_RUNTIME_TRULE_FORMAT_H
#define TAPAS_RUNTIME_TRULE_FORMAT_H
#include "tapas/tval.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Read-only Rule-family and Function representations; never evaluate captures. */
tstring *trule_format_abbr(void *self);
tstring *trule_format_full(void *self);
#ifdef __cplusplus
}
#endif
#endif
