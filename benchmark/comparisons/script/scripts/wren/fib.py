# wren/fib.py -- Wren's test/benchmark/fib.py: naive recursive Fibonacci, run
# five times. argv[1] is n (Wren: 28). Prints fib(n) five times, then the
# elapsed ns.

import sys
import time


def fib(n):
    if n < 2: return n
    return fib(n - 1) + fib(n - 2)


def main(n):
    for i in range(0, 5):
        print(fib(n))


count = int(sys.argv[1])

t0 = time.perf_counter_ns()
main(count)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
