-- fib_recursive.lua -- naive recursive Fibonacci, measures function-call overhead.

local function fib(n)
    if n < 2 then return n end
    return fib(n - 2) + fib(n - 1)
end

local count = tonumber(arg[1])

local t0 = os.clock()
local result = fib(count)
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
