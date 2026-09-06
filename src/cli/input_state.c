/**
 * @file input_state.c
 * @brief Implements incremental delimiter tracking for the Tapas CLI.
 * @details Tracks brackets and quoted strings so the interactive reader can
 * decide whether a multiline command is complete.
 * @note This implementation is private to the command-line program and does
 * not perform full Tapas lexical analysis.
 */
#include "input_state.h"

void tinput_state_init(tinput_state *state)
{
	*state = (tinput_state){ 0 };
}

int tinput_state_complete(const tinput_state *state)
{
	return state->parenthesis == 0 && state->bracket == 0 &&
		state->curlybrace == 0 && state->singlequote == 0 &&
		state->doublequote == 0;
}

void tinput_state_update(tinput_state *state, char character)
{
	int outside_string = state->singlequote == 0 && state->doublequote == 0;
	switch (character) {
	case '(':
		if (outside_string) state->parenthesis++;
		break;
	case ')':
		if (outside_string) state->parenthesis--;
		break;
	case '[':
		if (outside_string) state->bracket++;
		break;
	case ']':
		if (outside_string) state->bracket--;
		break;
	case '{':
		if (outside_string) state->curlybrace++;
		break;
	case '}':
		if (outside_string) state->curlybrace--;
		break;
	case '\'':
		if (state->doublequote == 0)
			state->singlequote = !state->singlequote;
		break;
	case '"':
		if (state->singlequote == 0)
			state->doublequote = !state->doublequote;
		break;
	default:
		break;
	}
}
