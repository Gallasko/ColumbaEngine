# clox/trees.py -- Crafting Interpreters' test/benchmark/trees.lox in Python:
# build a five-way tree of depth 8 once, then walk it argv[1] times (clox:
# 100). Prints the last walk's sum (122068), then the elapsed ns. The tree is
# built before the timer, as in clox.

import sys
import time


class Tree:
    def __init__(self, depth):
        self.depth = depth
        if depth > 0:
            self.a = Tree(depth - 1)
            self.b = Tree(depth - 1)
            self.c = Tree(depth - 1)
            self.d = Tree(depth - 1)
            self.e = Tree(depth - 1)

    def walk(self):
        if self.depth == 0: return 0
        return (self.depth
                + self.a.walk()
                + self.b.walk()
                + self.c.walk()
                + self.d.walk()
                + self.e.walk())


def main(tree, n):
    last = 0
    for i in range(n):
        last = tree.walk()
        if last != 122068: print("Error")
    print(last)


count = int(sys.argv[1])
tree = Tree(8)

t0 = time.perf_counter_ns()
main(tree, count)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
