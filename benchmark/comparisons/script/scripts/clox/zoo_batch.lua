-- clox/zoo_batch.lua -- Crafting Interpreters' test/benchmark/zoo_batch.lox
-- in Lua: zoo's six calls in batches of 10000 passes. clox runs batches for
-- ten seconds; here arg[1] is the number of batches (fixed work). Prints the
-- sum and the batches, then the elapsed ns.

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
  local batch = 0
  while batch < n do
    for i = 0, 9999 do
      sum = sum + zoo:ant()
                + zoo:banana()
                + zoo:tuna()
                + zoo:hay()
                + zoo:grass()
                + zoo:mouse()
    end
    batch = batch + 1
  end
  print(sum)
  print(batch)
end

local count = tonumber(arg[1])

local t0 = os.clock()
main(count)
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
