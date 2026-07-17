# compute_pi.py — Leibniz series approximation of pi.
# Reads `count` (iterations) from sys.argv[1].
# Prints self-measured elapsed ns as the last stdout line.

import sys
import time


def compute_pi(iterations):
    pi = 0.0
    i = 0
    while i < iterations:
        term = 1.0 / (2 * i + 1)
        even = i - (i // 2) * 2
        if even == 0:
            pi += term
        if even == 1:
            pi -= term
        i += 1
    return 4 * pi


count = int(sys.argv[1])

t0 = time.perf_counter_ns()
result = compute_pi(count)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
