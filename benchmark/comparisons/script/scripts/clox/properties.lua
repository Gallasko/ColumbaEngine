-- clox/properties.lua -- Crafting Interpreters' test/benchmark/properties.lox in Lua:
-- field and method lookup: 30 calls a pass to methods that each return a different field.
-- arg[1] is the number of passes (clox: 500000). Prints the passes done,
-- then the elapsed ns.

local Foo = {}
Foo.__index = Foo

function Foo.new()
  local o = setmetatable({}, Foo)
  o.field0 = 1
  o.field1 = 1
  o.field2 = 1
  o.field3 = 1
  o.field4 = 1
  o.field5 = 1
  o.field6 = 1
  o.field7 = 1
  o.field8 = 1
  o.field9 = 1
  o.field10 = 1
  o.field11 = 1
  o.field12 = 1
  o.field13 = 1
  o.field14 = 1
  o.field15 = 1
  o.field16 = 1
  o.field17 = 1
  o.field18 = 1
  o.field19 = 1
  o.field20 = 1
  o.field21 = 1
  o.field22 = 1
  o.field23 = 1
  o.field24 = 1
  o.field25 = 1
  o.field26 = 1
  o.field27 = 1
  o.field28 = 1
  o.field29 = 1
  return o
end

function Foo:method0() return self.field0 end
function Foo:method1() return self.field1 end
function Foo:method2() return self.field2 end
function Foo:method3() return self.field3 end
function Foo:method4() return self.field4 end
function Foo:method5() return self.field5 end
function Foo:method6() return self.field6 end
function Foo:method7() return self.field7 end
function Foo:method8() return self.field8 end
function Foo:method9() return self.field9 end
function Foo:method10() return self.field10 end
function Foo:method11() return self.field11 end
function Foo:method12() return self.field12 end
function Foo:method13() return self.field13 end
function Foo:method14() return self.field14 end
function Foo:method15() return self.field15 end
function Foo:method16() return self.field16 end
function Foo:method17() return self.field17 end
function Foo:method18() return self.field18 end
function Foo:method19() return self.field19 end
function Foo:method20() return self.field20 end
function Foo:method21() return self.field21 end
function Foo:method22() return self.field22 end
function Foo:method23() return self.field23 end
function Foo:method24() return self.field24 end
function Foo:method25() return self.field25 end
function Foo:method26() return self.field26 end
function Foo:method27() return self.field27 end
function Foo:method28() return self.field28 end
function Foo:method29() return self.field29 end

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
