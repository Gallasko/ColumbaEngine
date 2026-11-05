-- wren/for.lua -- Wren's test/benchmark/for.lua: fill a table with 0..n-1,
-- then sum it with a pairs() loop. arg[1] is n (Wren: 1000000). Prints the
-- sum, then the elapsed ns.

local function main(n)
  local list = {}
  for i = 0, n - 1 do
    list[i] = i
  end

  local sum = 0
  for k, i in pairs(list) do
    sum = sum + i
  end
  print(sum)
end

local count = tonumber(arg[1])

local t0 = os.clock()
main(count)
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
