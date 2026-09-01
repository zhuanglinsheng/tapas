#ifndef TAPAS_COMPILE_UNIT_H
#define TAPAS_COMPILE_UNIT_H

#include "tapas/tbasis.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Incremental delimiter state used only by the interactive input reader. */
typedef struct {
	int_lexs parenthesis;
	int_lexs bracket;
	int_lexs curlybrace;
	int_lexs singlequote;
	int_lexs doublequote;
} tunit_ctr;

void tunit_ctr_init(tunit_ctr *counter);
void tunit_ctr_restore(tunit_ctr *counter);
int tunit_ctr_out_of_single_string(const tunit_ctr *counter);
int tunit_ctr_out_of_double_string(const tunit_ctr *counter);
int tunit_ctr_out_of_string(const tunit_ctr *counter);
int tunit_ctr_out_of_parenthesis(const tunit_ctr *counter);
int tunit_ctr_out_of_bracket(const tunit_ctr *counter);
int tunit_ctr_out_of_curlybrace(const tunit_ctr *counter);
int tunit_ctr_independent(const tunit_ctr *counter);
int tunit_ctr_update(tunit_ctr *counter, char character);

#ifdef __cplusplus
}
#endif

#endif /* TAPAS_COMPILE_UNIT_H */
