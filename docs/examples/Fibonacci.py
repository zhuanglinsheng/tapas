from time import process_time


def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)


def main():
    n = 26
    start = process_time()
    result = fib(n)
    elapsed = process_time() - start

    print(f"fib({n}) =", result)
    print(f"elapsed seconds = {elapsed}")


if __name__ == "__main__":
    main()
