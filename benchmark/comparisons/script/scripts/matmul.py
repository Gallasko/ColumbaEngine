# matmul.py -- naive O(n^3) square-matrix multiplication, flat 1-D storage.
# argv[1] is the matrix size n. Init outside timed region.

import sys
import time


def make_matrix(n, offset):
    return [float((i + offset) % 17) for i in range(n * n)]


n = int(sys.argv[1])
A = make_matrix(n, 0)
B = make_matrix(n, 7)
C = [0.0] * (n * n)

t0 = time.perf_counter_ns()

for i in range(n):
    for j in range(n):
        s = 0.0
        for k in range(n):
            s += A[i * n + k] * B[k * n + j]
        C[i * n + j] = s

elapsed = time.perf_counter_ns() - t0
print(elapsed)
