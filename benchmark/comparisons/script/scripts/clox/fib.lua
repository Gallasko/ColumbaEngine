-- clox/fib.lua -- Crafting Interpreters' test/benchmark/fib.lox in Lua:
-- naive recursive Fibonacci. arg[1] is n (clox: 35). Prints fib(n), then the
-- elapsed ns.

local function fib(n)
  if n < 2 then return n end
  return fib(n - 2) + fib(n - 1)
end

local function main(n)
  print(fib(n))
end

local count = tonumber(arg[1])

local t0 = os.clock()
main(count)
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
