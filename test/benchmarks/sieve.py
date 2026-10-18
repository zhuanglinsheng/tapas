import math
import time


def primes_up_to(limit):
    is_prime = bytearray(b"\x01") * (limit + 1)
    is_prime[:2] = b"\x00\x00"
    is_prime[4::2] = bytes((limit - 4) // 2 + 1)
    for candidate in range(3, math.isqrt(limit) + 1, 2):
        if is_prime[candidate]:
            start = candidate * candidate
            is_prime[start::candidate] = bytes((limit - start) // candidate + 1)
    return is_prime.count(1)


def bench():
    started = time.process_time_ns()
    result = primes_up_to(20_000)

    elapsed = time.process_time_ns() - started
    print(result)
    print(elapsed)


if __name__ == "__main__":
    bench()
