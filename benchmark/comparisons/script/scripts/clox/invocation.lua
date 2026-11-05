-- clox/invocation.lua -- Crafting Interpreters' test/benchmark/invocation.lox in Lua:
-- method invocation: 30 calls a pass to empty methods.
-- arg[1] is the number of passes (clox: 500000). Prints the passes done,
-- then the elapsed ns.

local Foo = {}
Foo.__index = Foo

function Foo.new()
  local o = setmetatable({}, Foo)
  return o
end

function Foo:method0() end
function Foo:method1() end
function Foo:method2() end
function Foo:method3() end
function Foo:method4() end
function Foo:method5() end
function Foo:method6() end
function Foo:method7() end
function Foo:method8() end
function Foo:method9() end
function Foo:method10() end
function Foo:method11() end
function Foo:method12() end
function Foo:method13() end
function Foo:method14() end
function Foo:method15() end
function Foo:method16() end
function Foo:method17() end
function Foo:method18() end
function Foo:method19() end
function Foo:method20() end
function Foo:method21() end
function Foo:method22() end
function Foo:method23() end
function Foo:method24() end
function Foo:method25() end
function Foo:method26() end
function Foo:method27() end
function Foo:method28() end
function Foo:method29() end

local function main(n)
  local foo = Foo.new()
  local i = 0
  while i < n do
    foo:method0()
    foo:method1()
    foo:method2()
    foo:method3()
    foo:method4()
    foo:method5()
    foo:method6()
    foo:method7()
    foo:method8()
    foo:method9()
    foo:method10()
    foo:method11()
    foo:method12()
    foo:method13()
    foo:method14()
    foo:method15()
    foo:method16()
    foo:method17()
    foo:method18()
    foo:method19()
    foo:method20()
    foo:method21()
    foo:method22()
    foo:method23()
    foo:method24()
    foo:method25()
    foo:method26()
    foo:method27()
    foo:method28()
    foo:method29()
    i = i + 1
  end
  print(i)
end

local count = tonumber(arg[1])

local t0 = os.clock()
main(count)
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
