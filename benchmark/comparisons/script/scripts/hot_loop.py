# hot_loop.py -- tight integer loop summing 1..n.
# Body lives in a function so the loop variables are LOAD_FAST locals,
# not LOAD_GLOBAL globals (matches the .pg / .lua versions' scope rules).

import sys
import time


def hot_loop(n):
    sum_ = 0
    i = 1
    while i <= n:
        sum_ = sum_ + i
        i = i + 1
    return sum_


count = int(sys.argv[1])

t0 = time.perf_counter_ns()
result = hot_loop(count)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
