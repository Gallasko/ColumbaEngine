# clox/equality_count.py -- clox's equality.lox made to count, in Python: each
# of its 19 comparisons adds one to a count when true, and the values are
# locals swapped in pairs at the end of each pass (1 and 2, "str" and "stru",
# True and False). The whole loop is timed. argv[1] is the number of passes
# (clox: 10000000). Prints the count, then the elapsed ns. Python's
# `1 == True` is true, so its count is not the others' (4 a pass): 6 on even
# passes, 4 on odd ones.

import sys
import time


def main(n):
    one, two, nul = 1, 2, None
    s, stru = "str", "stru"
    yes, no = True, False
    count = 0

    for i in range(n):
        if one == one: count += 1
        if one == two: count += 1
        if one == nul: count += 1
        if one == s: count += 1
        if one == yes: count += 1
        if nul == nul: count += 1
        if nul == one: count += 1
        if nul == s: count += 1
        if nul == yes: count += 1
        if yes == yes: count += 1
        if yes == one: count += 1
        if yes == no: count += 1
        if yes == s: count += 1
        if yes == nul: count += 1
        if s == s: count += 1
        if s == stru: count += 1
        if s == one: count += 1
        if s == nul: count += 1
        if s == yes: count += 1

        one, two = two, one
        s, stru = stru, s
        yes, no = no, yes
    print(count)

count = int(sys.argv[1])

t0 = time.perf_counter_ns()
main(count)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
