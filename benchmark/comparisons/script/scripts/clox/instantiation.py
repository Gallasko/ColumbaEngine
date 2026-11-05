# clox/instantiation.py -- Crafting Interpreters' test/benchmark/
# instantiation.lox in Python: creating instances and calling an empty
# __init__, 30 a pass. argv[1] is the number of passes (clox: 500000). Prints
# the passes done, then the elapsed ns.

import sys
import time


class Foo:
    def __init__(self):
        pass


def main(n):
    i = 0
    while i < n:
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        Foo()
        i = i + 1
    print(i)

count = int(sys.argv[1])

t0 = time.perf_counter_ns()
main(count)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
