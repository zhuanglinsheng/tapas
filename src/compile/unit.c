#include "tapas/compile/unit.h"

void tunit_ctr_init(tunit_ctr *counter)
{
	*counter = (tunit_ctr){ 0 };
}

void tunit_ctr_restore(tunit_ctr *counter)
{
	tunit_ctr_init(counter);
}

int tunit_ctr_out_of_single_string(const tunit_ctr *counter)
{
	return counter->singlequote == 0;
}

int tunit_ctr_out_of_double_string(const tunit_ctr *counter)
{
	return counter->doublequote == 0;
}

int tunit_ctr_out_of_string(const tunit_ctr *counter)
{
	return tunit_ctr_out_of_single_string(counter) &&
		tunit_ctr_out_of_double_string(counter);
}

int tunit_ctr_out_of_parenthesis(const tunit_ctr *counter)
{
	return counter->parenthesis == 0;
}

int tunit_ctr_out_of_bracket(const tunit_ctr *counter)
{
	return counter->bracket == 0;
}

int tunit_ctr_out_of_curlybrace(const tunit_ctr *counter)
{
	return counter->curlybrace == 0;
}

int tunit_ctr_independent(const tunit_ctr *counter)
{
	return counter->parenthesis == 0 && counter->bracket == 0 &&
		counter->curlybrace == 0 && counter->singlequote == 0 &&
		counter->doublequote == 0;
}

int tunit_ctr_update(tunit_ctr *counter, char character)
{
	int outside_string = counter->singlequote == 0 &&
		counter->doublequote == 0;
	switch (character) {
	case '(':
		if (outside_string) counter->parenthesis++;
		break;
	case ')':
		if (outside_string) counter->parenthesis--;
		break;
	case '[':
		if (outside_string) counter->bracket++;
		break;
	case ']':
		if (outside_string) counter->bracket--;
		break;
	case '{':
		if (outside_string) counter->curlybrace++;
		break;
	case '}':
		if (outside_string) counter->curlybrace--;
		break;
	case '\'':
		if (counter->doublequote == 0)
			counter->singlequote = !counter->singlequote;
		break;
	case '"':
		if (counter->singlequote == 0)
			counter->doublequote = !counter->doublequote;
		break;
	default:
		break;
	}
	return 1;
}
