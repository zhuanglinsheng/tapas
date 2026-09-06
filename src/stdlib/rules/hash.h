/**
 * @file hash.h
 * @brief Declares private hashing operations for the rules package.
 * @details Exposes RuleIR fingerprints and native package entry points only to
 * other implementation files in the rules package.
 * @note This is not a core runtime header or a cross-package C API.
 */
#ifndef TAPAS_STDLIB_RULES_HASH_H
#define TAPAS_STDLIB_RULES_HASH_H

#include "tapas/objects/trule_ir.h"

uint64_t trule_ir_semantic_hash(const trule_ir *ir);
uint64_t trule_ir_content_hash(const trule_ir *ir);
void trules_semantic_hash(tobj *params, uint_regs count, tobj *result);
void trules_content_hash(tobj *params, uint_regs count, tobj *result);

#endif
