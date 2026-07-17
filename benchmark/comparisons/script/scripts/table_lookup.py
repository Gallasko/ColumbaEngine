# table_lookup.py -- N inserts of (i -> i*2) into a dict, then N sequential
# lookups summing the values. Lookup loop in a function so loop vars are locals.

import sys
import time


def build_dict(n):
    d = {}
    for i in range(n):
        d[i] = i * 2
    return d


def sum_lookup(d, n):
    total = 0
    for i in range(n):
        total += d[i]
    return total


count = int(sys.argv[1])
d = build_dict(count)

t0 = time.perf_counter_ns()
total = sum_lookup(d, count)
elapsed = time.perf_counter_ns() - t0
print(elapsed)
