-- clox/zoo.lua -- Crafting Interpreters' test/benchmark/zoo.lox in Lua: six
-- method calls a pass, each returning a field, until their sum reaches arg[1]
-- (clox: 10000000). Prints the sum, then the elapsed ns.

local Zoo = {}
Zoo.__index = Zoo

function Zoo.new()
  local o = setmetatable({}, Zoo)
  o.aarvark = 1
  o.baboon = 1
  o.cat = 1
  o.donkey = 1
  o.elephant = 1
  o.fox = 1
  return o
end

function Zoo:ant() return self.aarvark end
function Zoo:banana() return self.baboon end
function Zoo:tuna() return self.cat end
function Zoo:hay() return self.donkey end
function Zoo:grass() return self.elephant end
function Zoo:mouse() return self.fox end

local function main(n)
  local zoo = Zoo.new()
  local sum = 0
  while sum < n do
    sum = sum + zoo:ant()
              + zoo:banana()
              + zoo:tuna()
              + zoo:hay()
              + zoo:grass()
              + zoo:mouse()
  end
  print(sum)
end

local count = tonumber(arg[1])

local t0 = os.clock()
main(count)
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
