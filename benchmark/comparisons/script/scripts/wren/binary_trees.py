# wren/binary_trees.py -- Wren's test/benchmark/binary_trees.py (Benchmarks
# Game, by Antoine Pitrou, Dominique Wahli, Heinrich Acker). The driver is in
# main() so its variables are locals. argv[1] is max_depth (Wren: 12).
# Prints each check, then the elapsed ns.

import sys
import time


def make_tree(item, depth):
    if not depth: return item, None, None
    item2 = item + item
    depth -= 1
    return item, make_tree(item2 - 1, depth), make_tree(item2, depth)


def check_tree(node):
    item, left, right = node
    if not left: return item
    return item + check_tree(left) - check_tree(right)


def main(max_depth):
    min_depth = 4
    stretch_depth = max_depth + 1

    print(check_tree(make_tree(0, stretch_depth)))

    long_lived_tree = make_tree(0, max_depth)

    iterations = 2 ** max_depth
    for depth in range(min_depth, stretch_depth, 2):
        check = 0
        for i in range(1, iterations + 1):
            check += check_tree(make_tree(i, depth)) + check_tree(make_tree(-i, depth))
        print(check)
        iterations //= 4

    print(check_tree(long_lived_tree))


count = int(sys.argv[1])

t0 = time.perf_counter_ns()
main(count)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
