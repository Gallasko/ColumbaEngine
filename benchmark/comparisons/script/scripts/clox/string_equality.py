# clox/string_equality.py -- Crafting Interpreters' test/benchmark/
# string_equality.lox in Python: == between eight 64-character strings that
# differ only in the last character, 640 a pass, each result thrown away. The
# first loop only reads the operands; the result is the second loop's time
# minus the first's. argv[1] is the number of passes (clox: 100000). Prints
# the passes, then the equality ns.

import sys
import time


def main(n):
    a1 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1"
    a2 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa2"
    a3 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa3"
    a4 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa4"
    a5 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa5"
    a6 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa6"
    a7 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa7"
    a8 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa8"
    i = 0

    loop_start = time.perf_counter_ns()
    while i < n:
        i = i + 1

        a1; a1; a1; a2; a1; a3; a1; a4; a1; a5; a1; a6; a1; a7; a1; a8
        a2; a1; a2; a2; a2; a3; a2; a4; a2; a5; a2; a6; a2; a7; a2; a8
        a3; a1; a3; a2; a3; a3; a3; a4; a3; a5; a3; a6; a3; a7; a3; a8
        a4; a1; a4; a2; a4; a3; a4; a4; a4; a5; a4; a6; a4; a7; a4; a8
        a5; a1; a5; a2; a5; a3; a5; a4; a5; a5; a5; a6; a5; a7; a5; a8
        a6; a1; a6; a2; a6; a3; a6; a4; a6; a5; a6; a6; a6; a7; a6; a8
        a7; a1; a7; a2; a7; a3; a7; a4; a7; a5; a7; a6; a7; a7; a7; a8
        a8; a1; a8; a2; a8; a3; a8; a4; a8; a5; a8; a6; a8; a7; a8; a8

        a1; a1; a1; a2; a1; a3; a1; a4; a1; a5; a1; a6; a1; a7; a1; a8
        a2; a1; a2; a2; a2; a3; a2; a4; a2; a5; a2; a6; a2; a7; a2; a8
        a3; a1; a3; a2; a3; a3; a3; a4; a3; a5; a3; a6; a3; a7; a3; a8
        a4; a1; a4; a2; a4; a3; a4; a4; a4; a5; a4; a6; a4; a7; a4; a8
        a5; a1; a5; a2; a5; a3; a5; a4; a5; a5; a5; a6; a5; a7; a5; a8
        a6; a1; a6; a2; a6; a3; a6; a4; a6; a5; a6; a6; a6; a7; a6; a8
        a7; a1; a7; a2; a7; a3; a7; a4; a7; a5; a7; a6; a7; a7; a7; a8
        a8; a1; a8; a2; a8; a3; a8; a4; a8; a5; a8; a6; a8; a7; a8; a8

        a1; a1; a1; a2; a1; a3; a1; a4; a1; a5; a1; a6; a1; a7; a1; a8
        a2; a1; a2; a2; a2; a3; a2; a4; a2; a5; a2; a6; a2; a7; a2; a8
        a3; a1; a3; a2; a3; a3; a3; a4; a3; a5; a3; a6; a3; a7; a3; a8
        a4; a1; a4; a2; a4; a3; a4; a4; a4; a5; a4; a6; a4; a7; a4; a8
        a5; a1; a5; a2; a5; a3; a5; a4; a5; a5; a5; a6; a5; a7; a5; a8
        a6; a1; a6; a2; a6; a3; a6; a4; a6; a5; a6; a6; a6; a7; a6; a8
        a7; a1; a7; a2; a7; a3; a7; a4; a7; a5; a7; a6; a7; a7; a7; a8
        a8; a1; a8; a2; a8; a3; a8; a4; a8; a5; a8; a6; a8; a7; a8; a8

        a1; a1; a1; a2; a1; a3; a1; a4; a1; a5; a1; a6; a1; a7; a1; a8
        a2; a1; a2; a2; a2; a3; a2; a4; a2; a5; a2; a6; a2; a7; a2; a8
        a3; a1; a3; a2; a3; a3; a3; a4; a3; a5; a3; a6; a3; a7; a3; a8
        a4; a1; a4; a2; a4; a3; a4; a4; a4; a5; a4; a6; a4; a7; a4; a8
        a5; a1; a5; a2; a5; a3; a5; a4; a5; a5; a5; a6; a5; a7; a5; a8
        a6; a1; a6; a2; a6; a3; a6; a4; a6; a5; a6; a6; a6; a7; a6; a8
        a7; a1; a7; a2; a7; a3; a7; a4; a7; a5; a7; a6; a7; a7; a7; a8
        a8; a1; a8; a2; a8; a3; a8; a4; a8; a5; a8; a6; a8; a7; a8; a8

        a1; a1; a1; a2; a1; a3; a1; a4; a1; a5; a1; a6; a1; a7; a1; a8
        a2; a1; a2; a2; a2; a3; a2; a4; a2; a5; a2; a6; a2; a7; a2; a8
        a3; a1; a3; a2; a3; a3; a3; a4; a3; a5; a3; a6; a3; a7; a3; a8
        a4; a1; a4; a2; a4; a3; a4; a4; a4; a5; a4; a6; a4; a7; a4; a8
        a5; a1; a5; a2; a5; a3; a5; a4; a5; a5; a5; a6; a5; a7; a5; a8
        a6; a1; a6; a2; a6; a3; a6; a4; a6; a5; a6; a6; a6; a7; a6; a8
        a7; a1; a7; a2; a7; a3; a7; a4; a7; a5; a7; a6; a7; a7; a7; a8
        a8; a1; a8; a2; a8; a3; a8; a4; a8; a5; a8; a6; a8; a7; a8; a8

        a1; a1; a1; a2; a1; a3; a1; a4; a1; a5; a1; a6; a1; a7; a1; a8
        a2; a1; a2; a2; a2; a3; a2; a4; a2; a5; a2; a6; a2; a7; a2; a8
        a3; a1; a3; a2; a3; a3; a3; a4; a3; a5; a3; a6; a3; a7; a3; a8
        a4; a1; a4; a2; a4; a3; a4; a4; a4; a5; a4; a6; a4; a7; a4; a8
        a5; a1; a5; a2; a5; a3; a5; a4; a5; a5; a5; a6; a5; a7; a5; a8
        a6; a1; a6; a2; a6; a3; a6; a4; a6; a5; a6; a6; a6; a7; a6; a8
        a7; a1; a7; a2; a7; a3; a7; a4; a7; a5; a7; a6; a7; a7; a7; a8
        a8; a1; a8; a2; a8; a3; a8; a4; a8; a5; a8; a6; a8; a7; a8; a8

        a1; a1; a1; a2; a1; a3; a1; a4; a1; a5; a1; a6; a1; a7; a1; a8
        a2; a1; a2; a2; a2; a3; a2; a4; a2; a5; a2; a6; a2; a7; a2; a8
        a3; a1; a3; a2; a3; a3; a3; a4; a3; a5; a3; a6; a3; a7; a3; a8
        a4; a1; a4; a2; a4; a3; a4; a4; a4; a5; a4; a6; a4; a7; a4; a8
        a5; a1; a5; a2; a5; a3; a5; a4; a5; a5; a5; a6; a5; a7; a5; a8
        a6; a1; a6; a2; a6; a3; a6; a4; a6; a5; a6; a6; a6; a7; a6; a8
        a7; a1; a7; a2; a7; a3; a7; a4; a7; a5; a7; a6; a7; a7; a7; a8
        a8; a1; a8; a2; a8; a3; a8; a4; a8; a5; a8; a6; a8; a7; a8; a8

        a1; a1; a1; a2; a1; a3; a1; a4; a1; a5; a1; a6; a1; a7; a1; a8
        a2; a1; a2; a2; a2; a3; a2; a4; a2; a5; a2; a6; a2; a7; a2; a8
        a3; a1; a3; a2; a3; a3; a3; a4; a3; a5; a3; a6; a3; a7; a3; a8
        a4; a1; a4; a2; a4; a3; a4; a4; a4; a5; a4; a6; a4; a7; a4; a8
        a5; a1; a5; a2; a5; a3; a5; a4; a5; a5; a5; a6; a5; a7; a5; a8
        a6; a1; a6; a2; a6; a3; a6; a4; a6; a5; a6; a6; a6; a7; a6; a8
        a7; a1; a7; a2; a7; a3; a7; a4; a7; a5; a7; a6; a7; a7; a7; a8
        a8; a1; a8; a2; a8; a3; a8; a4; a8; a5; a8; a6; a8; a7; a8; a8

        a1; a1; a1; a2; a1; a3; a1; a4; a1; a5; a1; a6; a1; a7; a1; a8
        a2; a1; a2; a2; a2; a3; a2; a4; a2; a5; a2; a6; a2; a7; a2; a8
        a3; a1; a3; a2; a3; a3; a3; a4; a3; a5; a3; a6; a3; a7; a3; a8
        a4; a1; a4; a2; a4; a3; a4; a4; a4; a5; a4; a6; a4; a7; a4; a8
        a5; a1; a5; a2; a5; a3; a5; a4; a5; a5; a5; a6; a5; a7; a5; a8
        a6; a1; a6; a2; a6; a3; a6; a4; a6; a5; a6; a6; a6; a7; a6; a8
        a7; a1; a7; a2; a7; a3; a7; a4; a7; a5; a7; a6; a7; a7; a7; a8
        a8; a1; a8; a2; a8; a3; a8; a4; a8; a5; a8; a6; a8; a7; a8; a8

        a1; a1; a1; a2; a1; a3; a1; a4; a1; a5; a1; a6; a1; a7; a1; a8
        a2; a1; a2; a2; a2; a3; a2; a4; a2; a5; a2; a6; a2; a7; a2; a8
        a3; a1; a3; a2; a3; a3; a3; a4; a3; a5; a3; a6; a3; a7; a3; a8
        a4; a1; a4; a2; a4; a3; a4; a4; a4; a5; a4; a6; a4; a7; a4; a8
        a5; a1; a5; a2; a5; a3; a5; a4; a5; a5; a5; a6; a5; a7; a5; a8
        a6; a1; a6; a2; a6; a3; a6; a4; a6; a5; a6; a6; a6; a7; a6; a8
        a7; a1; a7; a2; a7; a3; a7; a4; a7; a5; a7; a6; a7; a7; a7; a8
        a8; a1; a8; a2; a8; a3; a8; a4; a8; a5; a8; a6; a8; a7; a8; a8
    loop_time = time.perf_counter_ns() - loop_start

    start = time.perf_counter_ns()
    i = 0
    while i < n:
        i = i + 1

        a1 == a1; a1 == a2; a1 == a3; a1 == a4; a1 == a5; a1 == a6; a1 == a7; a1 == a8
        a2 == a1; a2 == a2; a2 == a3; a2 == a4; a2 == a5; a2 == a6; a2 == a7; a2 == a8
        a3 == a1; a3 == a2; a3 == a3; a3 == a4; a3 == a5; a3 == a6; a3 == a7; a3 == a8
        a4 == a1; a4 == a2; a4 == a3; a4 == a4; a4 == a5; a4 == a6; a4 == a7; a4 == a8
        a5 == a1; a5 == a2; a5 == a3; a5 == a4; a5 == a5; a5 == a6; a5 == a7; a5 == a8
        a6 == a1; a6 == a2; a6 == a3; a6 == a4; a6 == a5; a6 == a6; a6 == a7; a6 == a8
        a7 == a1; a7 == a2; a7 == a3; a7 == a4; a7 == a5; a7 == a6; a7 == a7; a7 == a8
        a8 == a1; a8 == a2; a8 == a3; a8 == a4; a8 == a5; a8 == a6; a8 == a7; a8 == a8

        a1 == a1; a1 == a2; a1 == a3; a1 == a4; a1 == a5; a1 == a6; a1 == a7; a1 == a8
        a2 == a1; a2 == a2; a2 == a3; a2 == a4; a2 == a5; a2 == a6; a2 == a7; a2 == a8
        a3 == a1; a3 == a2; a3 == a3; a3 == a4; a3 == a5; a3 == a6; a3 == a7; a3 == a8
        a4 == a1; a4 == a2; a4 == a3; a4 == a4; a4 == a5; a4 == a6; a4 == a7; a4 == a8
        a5 == a1; a5 == a2; a5 == a3; a5 == a4; a5 == a5; a5 == a6; a5 == a7; a5 == a8
        a6 == a1; a6 == a2; a6 == a3; a6 == a4; a6 == a5; a6 == a6; a6 == a7; a6 == a8
        a7 == a1; a7 == a2; a7 == a3; a7 == a4; a7 == a5; a7 == a6; a7 == a7; a7 == a8
        a8 == a1; a8 == a2; a8 == a3; a8 == a4; a8 == a5; a8 == a6; a8 == a7; a8 == a8

        a1 == a1; a1 == a2; a1 == a3; a1 == a4; a1 == a5; a1 == a6; a1 == a7; a1 == a8
        a2 == a1; a2 == a2; a2 == a3; a2 == a4; a2 == a5; a2 == a6; a2 == a7; a2 == a8
        a3 == a1; a3 == a2; a3 == a3; a3 == a4; a3 == a5; a3 == a6; a3 == a7; a3 == a8
        a4 == a1; a4 == a2; a4 == a3; a4 == a4; a4 == a5; a4 == a6; a4 == a7; a4 == a8
        a5 == a1; a5 == a2; a5 == a3; a5 == a4; a5 == a5; a5 == a6; a5 == a7; a5 == a8
        a6 == a1; a6 == a2; a6 == a3; a6 == a4; a6 == a5; a6 == a6; a6 == a7; a6 == a8
        a7 == a1; a7 == a2; a7 == a3; a7 == a4; a7 == a5; a7 == a6; a7 == a7; a7 == a8
        a8 == a1; a8 == a2; a8 == a3; a8 == a4; a8 == a5; a8 == a6; a8 == a7; a8 == a8

        a1 == a1; a1 == a2; a1 == a3; a1 == a4; a1 == a5; a1 == a6; a1 == a7; a1 == a8
        a2 == a1; a2 == a2; a2 == a3; a2 == a4; a2 == a5; a2 == a6; a2 == a7; a2 == a8
        a3 == a1; a3 == a2; a3 == a3; a3 == a4; a3 == a5; a3 == a6; a3 == a7; a3 == a8
        a4 == a1; a4 == a2; a4 == a3; a4 == a4; a4 == a5; a4 == a6; a4 == a7; a4 == a8
        a5 == a1; a5 == a2; a5 == a3; a5 == a4; a5 == a5; a5 == a6; a5 == a7; a5 == a8
        a6 == a1; a6 == a2; a6 == a3; a6 == a4; a6 == a5; a6 == a6; a6 == a7; a6 == a8
        a7 == a1; a7 == a2; a7 == a3; a7 == a4; a7 == a5; a7 == a6; a7 == a7; a7 == a8
        a8 == a1; a8 == a2; a8 == a3; a8 == a4; a8 == a5; a8 == a6; a8 == a7; a8 == a8

        a1 == a1; a1 == a2; a1 == a3; a1 == a4; a1 == a5; a1 == a6; a1 == a7; a1 == a8
        a2 == a1; a2 == a2; a2 == a3; a2 == a4; a2 == a5; a2 == a6; a2 == a7; a2 == a8
        a3 == a1; a3 == a2; a3 == a3; a3 == a4; a3 == a5; a3 == a6; a3 == a7; a3 == a8
        a4 == a1; a4 == a2; a4 == a3; a4 == a4; a4 == a5; a4 == a6; a4 == a7; a4 == a8
        a5 == a1; a5 == a2; a5 == a3; a5 == a4; a5 == a5; a5 == a6; a5 == a7; a5 == a8
        a6 == a1; a6 == a2; a6 == a3; a6 == a4; a6 == a5; a6 == a6; a6 == a7; a6 == a8
        a7 == a1; a7 == a2; a7 == a3; a7 == a4; a7 == a5; a7 == a6; a7 == a7; a7 == a8
        a8 == a1; a8 == a2; a8 == a3; a8 == a4; a8 == a5; a8 == a6; a8 == a7; a8 == a8

        a1 == a1; a1 == a2; a1 == a3; a1 == a4; a1 == a5; a1 == a6; a1 == a7; a1 == a8
        a2 == a1; a2 == a2; a2 == a3; a2 == a4; a2 == a5; a2 == a6; a2 == a7; a2 == a8
        a3 == a1; a3 == a2; a3 == a3; a3 == a4; a3 == a5; a3 == a6; a3 == a7; a3 == a8
        a4 == a1; a4 == a2; a4 == a3; a4 == a4; a4 == a5; a4 == a6; a4 == a7; a4 == a8
        a5 == a1; a5 == a2; a5 == a3; a5 == a4; a5 == a5; a5 == a6; a5 == a7; a5 == a8
        a6 == a1; a6 == a2; a6 == a3; a6 == a4; a6 == a5; a6 == a6; a6 == a7; a6 == a8
        a7 == a1; a7 == a2; a7 == a3; a7 == a4; a7 == a5; a7 == a6; a7 == a7; a7 == a8
        a8 == a1; a8 == a2; a8 == a3; a8 == a4; a8 == a5; a8 == a6; a8 == a7; a8 == a8

        a1 == a1; a1 == a2; a1 == a3; a1 == a4; a1 == a5; a1 == a6; a1 == a7; a1 == a8
        a2 == a1; a2 == a2; a2 == a3; a2 == a4; a2 == a5; a2 == a6; a2 == a7; a2 == a8
        a3 == a1; a3 == a2; a3 == a3; a3 == a4; a3 == a5; a3 == a6; a3 == a7; a3 == a8
        a4 == a1; a4 == a2; a4 == a3; a4 == a4; a4 == a5; a4 == a6; a4 == a7; a4 == a8
        a5 == a1; a5 == a2; a5 == a3; a5 == a4; a5 == a5; a5 == a6; a5 == a7; a5 == a8
        a6 == a1; a6 == a2; a6 == a3; a6 == a4; a6 == a5; a6 == a6; a6 == a7; a6 == a8
        a7 == a1; a7 == a2; a7 == a3; a7 == a4; a7 == a5; a7 == a6; a7 == a7; a7 == a8
        a8 == a1; a8 == a2; a8 == a3; a8 == a4; a8 == a5; a8 == a6; a8 == a7; a8 == a8

        a1 == a1; a1 == a2; a1 == a3; a1 == a4; a1 == a5; a1 == a6; a1 == a7; a1 == a8
        a2 == a1; a2 == a2; a2 == a3; a2 == a4; a2 == a5; a2 == a6; a2 == a7; a2 == a8
        a3 == a1; a3 == a2; a3 == a3; a3 == a4; a3 == a5; a3 == a6; a3 == a7; a3 == a8
        a4 == a1; a4 == a2; a4 == a3; a4 == a4; a4 == a5; a4 == a6; a4 == a7; a4 == a8
        a5 == a1; a5 == a2; a5 == a3; a5 == a4; a5 == a5; a5 == a6; a5 == a7; a5 == a8
        a6 == a1; a6 == a2; a6 == a3; a6 == a4; a6 == a5; a6 == a6; a6 == a7; a6 == a8
        a7 == a1; a7 == a2; a7 == a3; a7 == a4; a7 == a5; a7 == a6; a7 == a7; a7 == a8
        a8 == a1; a8 == a2; a8 == a3; a8 == a4; a8 == a5; a8 == a6; a8 == a7; a8 == a8

        a1 == a1; a1 == a2; a1 == a3; a1 == a4; a1 == a5; a1 == a6; a1 == a7; a1 == a8
        a2 == a1; a2 == a2; a2 == a3; a2 == a4; a2 == a5; a2 == a6; a2 == a7; a2 == a8
        a3 == a1; a3 == a2; a3 == a3; a3 == a4; a3 == a5; a3 == a6; a3 == a7; a3 == a8
        a4 == a1; a4 == a2; a4 == a3; a4 == a4; a4 == a5; a4 == a6; a4 == a7; a4 == a8
        a5 == a1; a5 == a2; a5 == a3; a5 == a4; a5 == a5; a5 == a6; a5 == a7; a5 == a8
        a6 == a1; a6 == a2; a6 == a3; a6 == a4; a6 == a5; a6 == a6; a6 == a7; a6 == a8
        a7 == a1; a7 == a2; a7 == a3; a7 == a4; a7 == a5; a7 == a6; a7 == a7; a7 == a8
        a8 == a1; a8 == a2; a8 == a3; a8 == a4; a8 == a5; a8 == a6; a8 == a7; a8 == a8

        a1 == a1; a1 == a2; a1 == a3; a1 == a4; a1 == a5; a1 == a6; a1 == a7; a1 == a8
        a2 == a1; a2 == a2; a2 == a3; a2 == a4; a2 == a5; a2 == a6; a2 == a7; a2 == a8
        a3 == a1; a3 == a2; a3 == a3; a3 == a4; a3 == a5; a3 == a6; a3 == a7; a3 == a8
        a4 == a1; a4 == a2; a4 == a3; a4 == a4; a4 == a5; a4 == a6; a4 == a7; a4 == a8
        a5 == a1; a5 == a2; a5 == a3; a5 == a4; a5 == a5; a5 == a6; a5 == a7; a5 == a8
        a6 == a1; a6 == a2; a6 == a3; a6 == a4; a6 == a5; a6 == a6; a6 == a7; a6 == a8
        a7 == a1; a7 == a2; a7 == a3; a7 == a4; a7 == a5; a7 == a6; a7 == a7; a7 == a8
        a8 == a1; a8 == a2; a8 == a3; a8 == a4; a8 == a5; a8 == a6; a8 == a7; a8 == a8
    elapsed = time.perf_counter_ns() - start

    print(i)
    return elapsed - loop_time


count = int(sys.argv[1])
print(main(count))
