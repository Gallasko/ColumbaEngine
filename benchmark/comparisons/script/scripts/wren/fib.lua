-- wren/fib.lua -- Wren's test/benchmark/fib.lua: naive recursive Fibonacci,
-- run five times. fib is local here (Wren's file makes it a global).
-- arg[1] is n (Wren: 28). Prints fib(n) five times, then the elapsed ns.

local function fib(n)
  if n < 2 then return n end
  return fib(n - 1) + fib(n - 2)
end

local count = tonumber(arg[1])

local t0 = os.clock()
for i = 1, 5 do
  print(fib(count))
end
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
