/* Instantiated with SORT_PREFIX and SORT_COMPARE for each comparison domain.
 * Comparisons must not throw: insertion shifts temporarily hold an owned
 * value in a C local. The caller validates inputs and defers comparison errors. */
#define SORT_JOIN_INNER(a, b) a##b
#define SORT_JOIN(a, b) SORT_JOIN_INNER(a, b)
#define SORT_FN(suffix) SORT_JOIN(SORT_PREFIX, suffix)
#ifndef SORT_STORE
#define SORT_STORE(slot, value) ((slot) = (value))
#endif

#ifndef SORT_FALLBACK
static void SORT_FN(_sift)(tobj *data, uint_count parent, uint_count size)
{
	while (parent < size / 2) {
		uint_count child = parent * 2 + 1;
		if (child + 1 < size && SORT_COMPARE(&data[child], &data[child + 1]) < 0)
			child++;
		if (SORT_COMPARE(&data[parent], &data[child]) >= 0)
			break;
		sort_swap(&data[parent], &data[child]);
		parent = child;
	}
}

static void SORT_FN(_heap)(tobj *data, uint_count size)
{
	for (uint_count i = size / 2; i > 0; i--)
		SORT_FN(_sift)(data, i - 1, size);
	for (uint_count end = size; end > 1; end--) {
		sort_swap(&data[0], &data[end - 1]);
		SORT_FN(_sift)(data, 0, end - 1);
	}
}

static const tobj *SORT_FN(_median)(const tobj *a, const tobj *b, const tobj *c)
{
	if (SORT_COMPARE(a, b) > 0) {
		const tobj *temporary = a;
		a = b;
		b = temporary;
	}
	if (SORT_COMPARE(b, c) > 0) {
		const tobj *temporary = b;
		b = c;
		c = temporary;
	}
	return SORT_COMPARE(a, b) > 0 ? a : b;
}

#endif

/* Stop after at most n shifts. Nearly sorted inputs finish in linear work;
 * random inputs restore the held value before falling back to introsort. */
static int SORT_FN(_partial_insertion)(tobj *data, uint_count size, uint_count start)
{
	uint_count budget = size;
	for (uint_count i = start; i < size; i++) {
		tobj value = data[i];
		uint_count position = i;
		while (position && SORT_COMPARE(&value, &data[position - 1]) < 0) {
			if (!budget) {
				SORT_STORE(data[position], value);
				return 0;
			}
			SORT_STORE(data[position], data[position - 1]);
			position--;
			budget--;
		}
		SORT_STORE(data[position], value);
	}
	return 1;
}

#ifndef SORT_FALLBACK
#ifdef SORT_DOMINANT_EQUAL
/* A majority-equal sample predicts a short path for the matching run. */
static uint_count SORT_FN(_partition_equal)(tobj *data, uint_count size,
	const tobj *pivot, uint_count *lower)
{
	uint_count low = 0, upper = size, position = 0;
	while (position < upper) {
		int order = SORT_COMPARE(&data[position], pivot);
		if (!order)
			position++;
		else if (order < 0)
			sort_swap(&data[low++], &data[position++]);
		else
			sort_swap(&data[position], &data[--upper]);
	}
	*lower = low;
	return upper;
}
#endif

static void SORT_FN(_intro)(tobj *data, uint_count size, unsigned depth)
{
	while (size > 16) {
		if (!depth) {
			SORT_FN(_heap)(data, size);
			return;
		}
		depth--;
		const tobj *samples[9];
		const tobj *a = &data[0];
		const tobj *b = &data[size / 2];
		const tobj *c = &data[size - 1];
		/* Stratify samples, then jitter within each stratum. Fixed strides
		 * can alias periodic inputs and repeatedly select an extreme key. */
		if (size > 128) {
			uint_count width = size / 9;
			uint32_t state = size * 2654435761u;
			for (unsigned i = 0; i < 9; i++) {
				state = state * 1664525u + 1013904223u;
				uint_count offset = (uint_count)(((uint64_t)state * width) >> 32);
				samples[i] = &data[i * width + offset];
			}
			a = SORT_FN(_median)(samples[0], samples[1], samples[2]);
			b = SORT_FN(_median)(samples[3], samples[4], samples[5]);
			c = SORT_FN(_median)(samples[6], samples[7], samples[8]);
		}
#ifdef SORT_TWO_ENDED
		/* Repeated samples favor a three-way partition. Check every pair,
		 * not just equality with the pivot. */
		int repeated_sample = 0;
		if (size > 128) {
			for (unsigned i = 1; i < 9 && !repeated_sample; i++)
				for (unsigned j = 0; j < i; j++)
					if (!SORT_COMPARE(samples[i], samples[j])) {
						repeated_sample = 1;
						break;
					}
		} else {
			repeated_sample = !SORT_COMPARE(a, b) || !SORT_COMPARE(a, c) ||
				!SORT_COMPARE(b, c);
		}
#endif
		b = SORT_FN(_median)(a, b, c);
		tobj pivot = *b;

#ifdef SORT_DOMINANT_EQUAL
		int equal_first = 0;
		if (repeated_sample) {
			unsigned matches = 0;
			if (size > 128) {
				for (unsigned i = 0; i < 9; i++)
					matches += SORT_COMPARE(samples[i], &pivot) == 0;
				equal_first = matches > 4;
			} else {
				matches = (SORT_COMPARE(a, &pivot) == 0) +
					(SORT_COMPARE(b, &pivot) == 0) +
					(SORT_COMPARE(c, &pivot) == 0);
				equal_first = matches > 1;
			}
		}
#endif
		uint_count lower = 0, upper = size;
#ifdef SORT_TWO_ENDED
		if (!repeated_sample) {
			/* Distinct samples favor a balanced two-way partition. Equal keys
			 * may land on either side; duplicate samples use the three-way
			 * path below. The sampled pivot bounds both scans. */
			long left = 0, right = (long)size - 1;
			for (;;) {
				while (SORT_COMPARE(&data[left], &pivot) < 0)
					left++;
				while (SORT_COMPARE(&data[right], &pivot) > 0)
					right--;
				if (left >= right)
					break;
				sort_swap(&data[left++], &data[right--]);
			}
			/* Remove equal keys at the boundary before recursing. */
			lower = upper = (uint_count)left;
			while (lower && !SORT_COMPARE(&data[lower - 1], &pivot))
				lower--;
			while (upper < size && !SORT_COMPARE(&data[upper], &pivot))
				upper++;
		} else
#endif
		{
#ifdef SORT_DOMINANT_EQUAL
			if (equal_first) {
				upper = SORT_FN(_partition_equal)(data, size, &pivot, &lower);
			} else
#endif
			{
				uint_count position = 0;
				while (position < upper) {
					int order = SORT_COMPARE(&data[position], &pivot);
					if (order < 0)
						sort_swap(&data[lower++], &data[position++]);
					else if (order > 0)
						sort_swap(&data[position], &data[--upper]);
					else
						position++;
				}
			}
		}
		/* Recurse only into the smaller side, bounding the C stack by log n. */
		if (lower < size - upper) {
			SORT_FN(_intro)(data, lower, depth);
			data += upper;
			size -= upper;
		} else {
			SORT_FN(_intro)(data + upper, size - upper, depth);
			size = lower;
		}
	}
	for (uint_count i = 1; i < size; i++) {
		tobj value = data[i];
		uint_count position = i;
		while (position && SORT_COMPARE(&value, &data[position - 1]) < 0) {
			SORT_STORE(data[position], data[position - 1]);
			position--;
		}
		SORT_STORE(data[position], value);
	}
}

#endif

static void SORT_FN(_values)(tobj *data, uint_count size)
{
	if (size < 2)
		return;
#ifdef SORT_FALLBACK
	/* Expensive comparisons make duplicate preparatory work costly. Small
	 * inputs go directly to libc; sample large inputs before scanning runs. */
	if (size < 128) {
		SORT_FALLBACK(data, size);
		return;
	}
	unsigned ascending = 0, descending = 0;
	uint_count step = size / 8;
	for (uint_count position = step; position < size; position += step) {
		int order = SORT_COMPARE(&data[position - 1], &data[position]);
		ascending += order < 0;
		descending += order > 0;
		if (ascending > 2 && descending > 2) {
			SORT_FALLBACK(data, size);
			return;
		}
		if (size - position <= step)
			break;
	}
#endif
	/* Fully monotone inputs, including all-equal values, need only one scan. */
	int direction = 0;
	uint_count i;
	for (i = 1; i < size; i++) {
		int order = SORT_COMPARE(&data[i - 1], &data[i]);
		if (order) {
			int next = order > 0 ? 1 : -1;
			if (direction && direction != next)
				break;
			direction = next;
		}
	}
	if (i == size) {
		if (direction > 0)
			for (uint_count j = 0; j < size / 2; j++)
				sort_swap(&data[j], &data[size - j - 1]);
		return;
	}
	if (direction < 0 && i >= size / 8 &&
	    SORT_FN(_partial_insertion)(data, size, i))
		return;
#ifdef SORT_FALLBACK
	SORT_FALLBACK(data, size);
#else
	unsigned depth = 0;
	for (uint_count n = size; n > 1; n >>= 1)
		depth += 2;
	SORT_FN(_intro)(data, size, depth);
#endif
}

#undef SORT_FN
#undef SORT_JOIN
#undef SORT_JOIN_INNER
#undef SORT_FALLBACK
#undef SORT_PREFIX
#undef SORT_COMPARE
#undef SORT_TWO_ENDED
#undef SORT_DOMINANT_EQUAL
#undef SORT_STORE
