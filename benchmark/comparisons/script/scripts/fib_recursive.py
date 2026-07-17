# fib_recursive.py — naive recursive Fibonacci, measures function-call overhead.
# argv[1] is the n in fib(n). fib(30) does ~1.6M calls.

import sys
import time


def fib(n):
    if n < 2:
        return n
    return fib(n - 2) + fib(n - 1)


count = int(sys.argv[1])

t0 = time.perf_counter_ns()
result = fib(count)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
