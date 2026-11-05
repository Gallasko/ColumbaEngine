# wren/string_equals.py -- Wren's test/benchmark/string_equals.py: ten string
# comparisons a pass, equal and unequal, plus a string against a number.
# argv[1] is the number of passes (Wren: 1000000). Prints how many were
# equal, then the elapsed ns.

import sys
import time


def main(n):
    count = 0
    for i in range(0, n):
        if "abc" == "abc":
            count = count + 1
        if "a slightly longer string" == \
           "a slightly longer string":
            count = count + 1
        if "a significantly longer string but still not overwhelmingly long string" == \
           "a significantly longer string but still not overwhelmingly long string":
            count = count + 1

        if "" == "abc":
            count = count + 1
        if "abc" == "abcd":
            count = count + 1
        if "changed one character" == "changed !ne character":
            count = count + 1
        if "123" == 123: count = count + 1
        if "a slightly longer string" == \
           "a slightly longer string!":
            count = count + 1
        if "a slightly longer string" == \
           "a slightly longer strinh":
            count = count + 1
        if "a significantly longer string but still not overwhelmingly long string" == \
           "another":
            count = count + 1
    print(count)


count = int(sys.argv[1])

t0 = time.perf_counter_ns()
main(count)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
