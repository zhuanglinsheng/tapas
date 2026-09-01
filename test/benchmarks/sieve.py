import time


def primes_up_to(limit):
    is_prime = []
    for value in range(limit + 1):
        is_prime.append(value >= 2)

    candidate = 2
    while candidate * candidate <= limit:
        if is_prime[candidate]:
            multiple = candidate * candidate
            while multiple <= limit:
                is_prime[multiple] = False
                multiple = multiple + candidate
        candidate = candidate + 1

    count = 0
    for value in range(2, limit + 1):
        if is_prime[value]:
            count = count + 1
    return count


started = time.process_time_ns()
result = primes_up_to(20_000)

print(result)
print(time.process_time_ns() - started)
