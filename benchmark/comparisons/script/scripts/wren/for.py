# wren/for.py -- Wren's test/benchmark/for.py: fill a list with 0..n-1, then
# sum it with a for-in loop. argv[1] is n (Wren: 1000000). Prints the sum,
# then the elapsed ns.

import sys
import time


def main(n):
    list = []
    for i in range(0, n):
        list.append(i)

    sum = 0
    for i in list:
        sum += i
    print(sum)


count = int(sys.argv[1])

t0 = time.perf_counter_ns()
main(count)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
