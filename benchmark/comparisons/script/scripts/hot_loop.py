# hot_loop.py — tight integer loop summing 1..count.
# Measures interpreter dispatch + integer arithmetic, no allocation.

import sys
import time

count = int(sys.argv[1])

t0 = time.perf_counter_ns()

sum_ = 0
i = 1
while i <= count:
    sum_ = sum_ + i
    i = i + 1

elapsed = time.perf_counter_ns() - t0

print(elapsed)
