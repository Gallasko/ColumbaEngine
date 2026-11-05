-- wren/map_numeric.lua -- Wren's test/benchmark/map_numeric.lua: insert keys
-- 1..n into a table, read them back, then remove them. arg[1] is n (Wren:
-- 2000000). Prints the sum, then the elapsed ns.

local function main(n)
  local map = {}
  for i = 1, n do
    map[i] = i
  end

  local sum = 0
  for i = 1, n do
    sum = sum + map[i]
  end
  print(sum)

  for i = 1, n do
    map[i] = nil
  end
end

local count = tonumber(arg[1])

local t0 = os.clock()
main(count)
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
