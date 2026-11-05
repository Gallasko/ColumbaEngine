# clox/zoo_batch.py -- Crafting Interpreters' test/benchmark/zoo_batch.lox in
# Python: zoo's six calls in batches of 10000 passes. clox runs batches for
# ten seconds; here argv[1] is the number of batches (fixed work). Prints the
# sum and the batches, then the elapsed ns.

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
    batch = 0
    while batch < n:
        for i in range(0, 10000):
            sum = (sum + zoo.ant()
                       + zoo.banana()
                       + zoo.tuna()
                       + zoo.hay()
                       + zoo.grass()
                       + zoo.mouse())
        batch = batch + 1
    print(sum)
    print(batch)

count = int(sys.argv[1])

t0 = time.perf_counter_ns()
main(count)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
