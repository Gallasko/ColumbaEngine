# clox/zoo.py -- Crafting Interpreters' test/benchmark/zoo.lox in Python: six
# method calls a pass, each returning a field, until their sum reaches argv[1]
# (clox: 10000000). Prints the sum, then the elapsed ns.

import sys
import time


class Zoo:
    def __init__(self):
        self.aarvark = 1
        self.baboon = 1
        self.cat = 1
        self.donkey = 1
        self.elephant = 1
        self.fox = 1

    def ant(self): return self.aarvark
    def banana(self): return self.baboon
    def tuna(self): return self.cat
    def hay(self): return self.donkey
    def grass(self): return self.elephant
    def mouse(self): return self.fox


def main(n):
    zoo = Zoo()
    sum = 0
    while sum < n:
        sum = (sum + zoo.ant()
                   + zoo.banana()
                   + zoo.tuna()
                   + zoo.hay()
                   + zoo.grass()
                   + zoo.mouse())
    print(sum)

count = int(sys.argv[1])

t0 = time.perf_counter_ns()
main(count)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
