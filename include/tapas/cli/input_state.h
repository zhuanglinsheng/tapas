#ifndef TAPAS_CLI_INPUT_STATE_H
#define TAPAS_CLI_INPUT_STATE_H

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
} tinput_state;

void tinput_state_init(tinput_state *state);
int tinput_state_complete(const tinput_state *state);
void tinput_state_update(tinput_state *state, char character);

#ifdef __cplusplus
}
#endif

#endif
