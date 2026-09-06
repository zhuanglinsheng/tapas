#ifndef TAPAS_LSP_PRESENTATION_H
#define TAPAS_LSP_PRESENTATION_H
#include "tapas/compile/frontend.h"
#include "tapas/compile/module.h"
/* Owned plain-text descriptions. No evaluation or handwritten signature parsing. */
tstring *tlsp_present_source(const tfrontend *, const tsemantic_symbol *);
tstring *tlsp_present_standard(const tstandard_symbol *);
tstring *tlsp_present_field(const tstatic_type_arena *, const char *, tstatic_type_id);
tstring *tlsp_present_export(const tfrontend *, const tmodule_export *);
#endif
