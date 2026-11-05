# clox/binary_trees.py -- Crafting Interpreters' test/benchmark/
# binary_trees.lox in Python: a Tree class that stores item and depth and
# sets both children to None at a leaf. argv[1] is max_depth (clox: 14).
# Prints each check value on its own line, then the elapsed ns.

import sys
import time


class Tree:
    def __init__(self, item, depth):
        self.item = item
        self.depth = depth
        if depth > 0:
            item2 = item + item
            depth = depth - 1
            self.left = Tree(item2 - 1, depth)
            self.right = Tree(item2, depth)
        else:
            self.left = None
            self.right = None

    def check(self):
        if self.left is None:
            return self.item
        return self.item + self.left.check() - self.right.check()


def main(max_depth):
    min_depth = 4
    stretch_depth = max_depth + 1

    print(Tree(0, stretch_depth).check())

    long_lived_tree = Tree(0, max_depth)

    iterations = 1
    d = 0
    while d < max_depth:
        iterations = iterations * 2
        d = d + 1

    depth = min_depth
    while depth < stretch_depth:
        check = 0
        i = 1
        while i <= iterations:
            check = check + Tree(i, depth).check() + Tree(-i, depth).check()
            i = i + 1
        print(check)
        iterations = iterations // 4
        depth = depth + 2

    print(long_lived_tree.check())

count = int(sys.argv[1])

t0 = time.perf_counter_ns()
main(count)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
