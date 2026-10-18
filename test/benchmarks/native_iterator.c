/* Diagnostic only: native Iterator construction, validation and pooled release
 * versus the same argument update and checksum without an Iterator object. */
#include "tapas/objects/titer.h"
#include "tapas/textension.h"
#include "tapas/tstdlib.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static uint64_t cpu_ns(void)
{
	struct timespec value;
	assert(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &value) == 0);
	return (uint64_t)value.tv_sec * 1000000000 + value.tv_nsec;
}

int main(void)
{
	textension_symbol_ref found;
	assert(textension_find(tstdlib_descriptor(), nullptr, "iter", &found));
	tnative_function create = found.symbol->function;
	tobj params[3] = {{0}}, result = {0};
	tobj_set_int(&params[1], 3);
	tobj_set_int(&params[2], 20001);
	const unsigned repetitions = 1000000;
	uint64_t samples[2][11];
	for (unsigned run = 0; run <= 11; run++) {
		long checksums[2] = {0};
		for (unsigned pass = 0; pass < 2; pass++) {
			unsigned method = (run + pass) % 2;
			uint64_t started = cpu_ns();
			for (unsigned i = 0; i < repetitions; i++) {
				tobj_set_int(&params[0], i % 1024);
				if (method) {
					create(params, 3, &result);
					checksums[method] += ((titer *)result.val.v_tcompo)->start;
					tobj_try_clear(&result);
				} else {
					checksums[method] += params[0].val.v_tint;
				}
			}
			if (run)
				samples[method][run - 1] = cpu_ns() - started;
		}
		assert(checksums[0] == checksums[1]);
	}
	printf("{\"repetitions\":%u,", repetitions);
	for (unsigned method = 0; method < 2; method++) {
		printf("\"%s_ns\":[", method ? "iterator" : "baseline");
		for (unsigned i = 0; i < 11; i++)
			printf("%s%llu", i ? "," : "", (unsigned long long)samples[method][i]);
		printf("]%s", method ? "}\n" : ",");
	}
	return 0;
}
