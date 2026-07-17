# matmul.py -- naive O(n^3) square-matrix multiplication, flat 1-D storage.
# Multiply kernel in a function so the inner-loop variables are locals.

import sys
import time


def make_matrix(n, offset):
    return [float((i + offset) % 17) for i in range(n * n)]


def multiply(A, B, C, n):
    for i in range(n):
        for j in range(n):
            s = 0.0
            for k in range(n):
                s += A[i * n + k] * B[k * n + j]
            C[i * n + j] = s


n = int(sys.argv[1])
A = make_matrix(n, 0)
B = make_matrix(n, 7)
C = [0.0] * (n * n)

t0 = time.perf_counter_ns()
multiply(A, B, C, n)
elapsed = time.perf_counter_ns() - t0
print(elapsed)
