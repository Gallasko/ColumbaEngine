# binary_tree.py -- build a balanced binary tree of depth argv[1], then
# recursively count its nodes. Same shape as binary_tree.pg / .lua.

import sys
import time


class Node:
    # `d` (depth) is stored so countNodes can distinguish leaves via int==int.
    # PgScript can't compare instance==int or read unset fields, so all three
    # languages carry the depth field for a fair comparison.
    __slots__ = ("d", "left", "right")

    def __init__(self, depth):
        self.d = depth
        if depth > 0:
            self.left = Node(depth - 1)
            self.right = Node(depth - 1)


def count_nodes(node):
    if node.d == 0:
        return 1
    return 1 + count_nodes(node.left) + count_nodes(node.right)


count = int(sys.argv[1])

t0 = time.perf_counter_ns()
root = Node(count)
total = count_nodes(root)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
