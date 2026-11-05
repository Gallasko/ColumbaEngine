-- clox/instantiation.lua -- Crafting Interpreters' test/benchmark/
-- instantiation.lox in Lua: creating instances through a constructor that
-- calls an empty initializer, 30 a pass. arg[1] is the number of passes
-- (clox: 500000). Prints the passes done, then the elapsed ns.

local Foo = {}
Foo.__index = Foo

function Foo:init()
end

function Foo.new()
  local o = setmetatable({}, Foo)
  o:init()
  return o
end

local function main(n)
  local i = 0
  while i < n do
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    Foo.new()
    i = i + 1
  end
  print(i)
end

local count = tonumber(arg[1])

local t0 = os.clock()
main(count)
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
