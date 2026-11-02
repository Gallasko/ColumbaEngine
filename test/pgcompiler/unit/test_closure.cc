#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Helpers/algorithmmodule.h"
#include "Helpers/stringmodule.h"

#include <string>
#include <vector>

namespace pg
{
    namespace test
    {
        namespace
        {
            std::string runWith(const std::string& source, ScriptFrontEnd frontEnd, InterpretResult& result)
            {
                VM vm;

                vm.setFrontEnd(frontEnd);
                vm.addNativeModule("string", StringModule());
                vm.addNativeModule("algorithm", AlgorithmModule());

                result = vm.interpretFromText(source);

                return vm.testOutput;
            }

            // What the script prints, the same from both front-ends
            void expectPrints(const std::string& source, const std::string& expected)
            {
                for (auto frontEnd : {ScriptFrontEnd::Ast, ScriptFrontEnd::Pratt})
                {
                    InterpretResult result = InterpretResult::RUNTIME_ERROR;

                    const std::string output = runWith(source, frontEnd, result);

                    EXPECT_EQ(result, InterpretResult::OK) << source;
                    EXPECT_EQ(output, expected) << source;
                }
            }
        }

        TEST(ClosureCapture, AWriteReachesTheLocalWhileItsFunctionRuns)
        {
            expectPrints(
                "fun f()\n{\n    var calls = 0\n"
                "    var g = fun()\n    {\n        calls = calls + 1\n        return 1\n    }\n"
                "    g()\n    g()\n    return calls\n}\n"
                "__dprint(f())\n", "2\n");
        }

        TEST(ClosureCapture, AWriteInAClosureThatReturnsNothing)
        {
            expectPrints(
                "fun f()\n{\n    var calls = 0\n"
                "    var g = fun()\n    {\n        calls = calls + 1\n    }\n"
                "    g()\n    g()\n    __dprint(calls)\n}\n"
                "f()\n", "2\n");
        }

        TEST(ClosureCapture, TheAssignmentIsAnExpression)
        {
            expectPrints(
                "fun f()\n{\n    var a = 0\n"
                "    var g = fun()\n    {\n        var b = a = 5\n        return b + 1\n    }\n"
                "    __dprint(g())\n    __dprint(a)\n}\n"
                "f()\n", "6\n5\n");
        }

        TEST(ClosureCapture, ACompoundAssignmentWrites)
        {
            expectPrints(
                "fun f()\n{\n    var calls = 0\n"
                "    var g = fun()\n    {\n        calls += 2\n        return calls\n    }\n"
                "    g()\n    __dprint(g())\n    __dprint(calls)\n}\n"
                "f()\n", "4\n4\n");
        }

        TEST(ClosureCapture, IncrementChangesTheCapturedLocalAndNoOther)
        {
            expectPrints(
                "fun f()\n{\n    var calls = 0\n    var other = 5\n"
                "    var g = fun()\n    {\n        var mine = 100\n        calls++\n        __dprint(mine)\n    }\n"
                "    g()\n    g()\n    __dprint(other)\n    __dprint(calls)\n}\n"
                "f()\n", "100\n100\n5\n2\n");
        }

        TEST(ClosureCapture, DecrementChangesTheCapturedLocal)
        {
            expectPrints(
                "fun f()\n{\n    var left = 3\n"
                "    var g = fun()\n    {\n        left--\n    }\n"
                "    g()\n    __dprint(left)\n}\n"
                "f()\n", "2\n");
        }

        TEST(ClosureCapture, APostfixGivesTheOldValue)
        {
            expectPrints(
                "fun f()\n{\n    var n = 7\n"
                "    var g = fun()\n    {\n        var was = n++\n        return was\n    }\n"
                "    __dprint(g())\n    __dprint(n)\n}\n"
                "f()\n", "7\n8\n");
        }

        TEST(ClosureCapture, IncrementStopsOnWhatIsNoNumber)
        {
            for (auto frontEnd : {ScriptFrontEnd::Ast, ScriptFrontEnd::Pratt})
            {
                InterpretResult result = InterpretResult::OK;

                runWith("fun f()\n{\n    var s = \"a\"\n"
                        "    var g = fun()\n    {\n        s++\n    }\n"
                        "    g()\n}\n"
                        "f()\n", frontEnd, result);

                EXPECT_EQ(result, InterpretResult::RUNTIME_ERROR);
            }
        }

        TEST(ClosureCapture, AWriteOutlivesTheFunction)
        {
            expectPrints(
                "fun mk()\n{\n    var n = 0\n"
                "    return fun()\n    {\n        n = n + 1\n        return n\n    }\n}\n"
                "var c = mk()\nc()\nc()\n__dprint(c())\n"
                "var d = mk()\n__dprint(d())\n__dprint(c())\n", "3\n1\n4\n");
        }

        TEST(ClosureCapture, TwoClosuresShareTheLocal)
        {
            expectPrints(
                "fun mk()\n{\n    var n = 1\n"
                "    var set = fun(v)\n    {\n        n = v\n    }\n"
                "    var get = fun()\n    {\n        return n\n    }\n"
                "    set(2)\n    __dprint(get())\n    __dprint(n)\n    return [set, get]\n}\n"
                "var p = mk()\nvar s = p[0]\nvar g = p[1]\n"
                "s(3)\n__dprint(g())\n", "2\n2\n3\n");
        }

        TEST(ClosureCapture, AWriteThroughTwoClosures)
        {
            expectPrints(
                "fun f()\n{\n    var a = 0\n"
                "    var g = fun()\n    {\n"
                "        var h = fun()\n        {\n            a = a + 1\n        }\n"
                "        h()\n        h()\n    }\n"
                "    g()\n    g()\n    return a\n}\n"
                "__dprint(f())\n", "4\n");
        }

        TEST(ClosureCapture, AWriteToALocalOfALoopBody)
        {
            expectPrints(
                "fun f()\n{\n    var t = 0\n"
                "    for (var i = 0; i < 3; i++)\n    {\n        var k = i\n"
                "        var g = fun()\n        {\n            k = k + 10\n            t = t + k\n        }\n"
                "        g()\n        __dprint(k)\n    }\n    return t\n}\n"
                "__dprint(f())\n", "10\n11\n12\n33\n");
        }

        TEST(ClosureCapture, AWriteInALoopIsSeenByTheLoop)
        {
            expectPrints(
                "fun f()\n{\n    var total = 0\n"
                "    var add = fun(x)\n    {\n        total = total + x\n    }\n"
                "    for (var i = 0; i < 4; i++)\n    {\n        add(i)\n    }\n"
                "    return total\n}\n"
                "__dprint(f())\n", "6\n");
        }

        // What a closed upvalue holds stays alive however often it is read
        TEST(ClosureCapture, AClosedListSurvivesItsReads)
        {
            expectPrints(
                "import \"algorithm\"\n"
                "fun mk()\n{\n    var l = [1, 2]\n"
                "    return fun()\n    {\n        return l\n    }\n}\n"
                "var g = mk()\ng()\ng()\n"
                "var other = [7, 8, 9]\n"
                "__dprint(g()[0])\n__dprint(len(g()))\n__dprint(other[0])\n", "1\n2\n7\n");
        }

        // The upvalue takes its reference to what is stored, and lets go of what it held
        TEST(ClosureCapture, AStoredListOutlivesTheStack)
        {
            expectPrints(
                "import \"algorithm\"\n"
                "fun mk()\n{\n    var l = [1]\n"
                "    var set = fun(v)\n    {\n        l = v\n    }\n"
                "    var get = fun()\n    {\n        return l\n    }\n"
                "    return [set, get]\n}\n"
                "var p = mk()\nvar s = p[0]\nvar g = p[1]\n"
                "s([4, 5, 6])\n"
                "var other = [7, 8, 9]\n"
                "__dprint(g()[0])\n__dprint(len(g()))\n__dprint(other[0])\n", "4\n3\n7\n");
        }
    }
}
