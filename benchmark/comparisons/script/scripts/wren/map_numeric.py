# wren/map_numeric.py -- Wren's test/benchmark/map_numeric.py: insert keys
# 1..n into a dict, read them back, then remove them. argv[1] is n (Wren:
# 2000000). Prints the sum, then the elapsed ns.

import sys
import time


def main(n):
    map = {}
    for i in range(1, n + 1):
        map[i] = i

    sum = 0
    for i in range(1, n + 1):
        sum = sum + map[i]
    print(sum)

    for i in range(1, n + 1):
        del map[i]


count = int(sys.argv[1])

t0 = time.perf_counter_ns()
main(count)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
