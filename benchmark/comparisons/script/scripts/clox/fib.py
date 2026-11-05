# clox/fib.py -- Crafting Interpreters' test/benchmark/fib.lox in Python:
# naive recursive Fibonacci. argv[1] is n (clox: 35). Prints fib(n), then the
# elapsed ns.

import sys
import time


def fib(n):
    if n < 2: return n
    return fib(n - 2) + fib(n - 1)


def main(n):
    print(fib(n))

count = int(sys.argv[1])

t0 = time.perf_counter_ns()
main(count)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
