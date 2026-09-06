/**
 * @file codec.h
 * @brief Declares private persistence operations for the rules package.
 * @details Exposes TPIR encoding and native package entry points only to the
 * rules package implementation and its focused tests.
 * @note This is not a core runtime header or a cross-package C API.
 */
#ifndef TAPAS_STDLIB_RULES_CODEC_H
#define TAPAS_STDLIB_RULES_CODEC_H

#include "tapas/objects/trule_ir.h"

int trule_ir_serialize(const trule_ir *ir, tstring **result);
trule_ir *trule_ir_deserialize(const char *data);
void trules_serialize(tobj *params, uint_regs count, tobj *result);
void trules_deserialize(tobj *params, uint_regs count, tobj *result);

#endif
