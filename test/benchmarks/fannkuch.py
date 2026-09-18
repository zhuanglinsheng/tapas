import time


def fannkuch(n):
    perm1 = list(range(n))
    count = [0] * n
    checksum = 0
    maxflips = 0
    r = n
    while True:
        while r != 1:
            count[r - 1] = r
            r -= 1

        perm = perm1[:]
        flips = 0
        k = perm[0]
        while k != 0:
            i = 0
            j = k
            while i < j:
                perm[i], perm[j] = perm[j], perm[i]
                i += 1
                j -= 1
            flips += 1
            k = perm[0]
        checksum += flips
        if flips > maxflips:
            maxflips = flips

        while True:
            if r == n:
                return checksum + maxflips
            first = perm1[0]
            for i in range(r):
                perm1[i] = perm1[i + 1]
            perm1[r] = first
            count[r] -= 1
            if count[r] > 0:
                break
            r += 1


n = 8

started = time.process_time_ns()
result = fannkuch(n)
elapsed = time.process_time_ns() - started

print(result)
print(elapsed)
