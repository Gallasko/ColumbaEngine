# clox/method_call.py -- Crafting Interpreters' test/benchmark/method_call.lox
# in Python (the shootout methcall): dispatch through a class and a subclass, the body
# unrolled ten times. argv[1] is n (clox: 100000). Prints the two final
# values (true, false), then the elapsed ns.

import sys
import time


class Toggle(object):
    def __init__(self, start_state):
        self.bool = start_state

    def value(self):
        return self.bool

    def activate(self):
        self.bool = not self.bool
        return self


class NthToggle(Toggle):
    def __init__(self, start_state, max_counter):
        Toggle.__init__(self, start_state)
        self.count_max = max_counter
        self.counter = 0

    def activate(self):
        self.counter += 1
        if self.counter >= self.count_max:
            super(NthToggle, self).activate()
            self.counter = 0
        return self


def main(n):
    val = True
    toggle = Toggle(val)
    for i in range(0, n):
        val = toggle.activate().value()
        val = toggle.activate().value()
        val = toggle.activate().value()
        val = toggle.activate().value()
        val = toggle.activate().value()
        val = toggle.activate().value()
        val = toggle.activate().value()
        val = toggle.activate().value()
        val = toggle.activate().value()
        val = toggle.activate().value()
    print("true" if toggle.value() else "false")

    val = True
    ntoggle = NthToggle(val, 3)
    for i in range(0, n):
        val = ntoggle.activate().value()
        val = ntoggle.activate().value()
        val = ntoggle.activate().value()
        val = ntoggle.activate().value()
        val = ntoggle.activate().value()
        val = ntoggle.activate().value()
        val = ntoggle.activate().value()
        val = ntoggle.activate().value()
        val = ntoggle.activate().value()
        val = ntoggle.activate().value()
    print("true" if ntoggle.value() else "false")


count = int(sys.argv[1])

t0 = time.perf_counter_ns()
main(count)
elapsed = time.perf_counter_ns() - t0

print(elapsed)
