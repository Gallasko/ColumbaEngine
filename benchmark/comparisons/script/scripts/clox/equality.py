# clox/equality.py -- Crafting Interpreters' test/benchmark/equality.lox in
# Python: == between ints, None, booleans and strings, 19 a pass, each value
# thrown away. The first loop only evaluates the operands; the result is the
# second loop's time minus the first's. argv[1] is the number of passes (clox:
# 10000000). Prints the passes, then the equality ns.

import sys
import time


def main(n):
    i = 0

    loop_start = time.perf_counter_ns()
    while i < n:
        i = i + 1

        1; 1; 1; 2; 1; None; 1; "str"; 1; True
        None; None; None; 1; None; "str"; None; True
        True; True; True; 1; True; False; True; "str"; True; None
        "str"; "str"; "str"; "stru"; "str"; 1; "str"; None; "str"; True
    loop_time = time.perf_counter_ns() - loop_start

    start = time.perf_counter_ns()
    i = 0
    while i < n:
        i = i + 1

        1 == 1; 1 == 2; 1 == None; 1 == "str"; 1 == True
        None == None; None == 1; None == "str"; None == True
        True == True; True == 1; True == False; True == "str"; True == None
        "str" == "str"; "str" == "stru"; "str" == 1; "str" == None; "str" == True
    elapsed = time.perf_counter_ns() - start

    print(i)
    return elapsed - loop_time


count = int(sys.argv[1])
print(main(count))
