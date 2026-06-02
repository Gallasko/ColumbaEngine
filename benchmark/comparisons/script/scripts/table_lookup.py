# table_lookup.py -- N inserts of (i -> i*2) into a dict, then N sequential
# lookups summing the values. Only the lookup loop is timed.

import sys
import time

count = int(sys.argv[1])
d = {}
for i in range(count):
    d[i] = i * 2

t0 = time.perf_counter_ns()

total = 0
for i in range(count):
    total += d[i]

elapsed = time.perf_counter_ns() - t0
print(elapsed)
