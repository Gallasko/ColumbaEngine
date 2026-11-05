-- clox/trees.lua -- Crafting Interpreters' test/benchmark/trees.lox in Lua:
-- build a five-way tree of depth 8 once, then walk it arg[1] times (clox:
-- 100). Prints the last walk's sum (122068), then the elapsed ns. The tree
-- is built before the timer, as in clox.

local Tree = {}
Tree.__index = Tree

function Tree.new(depth)
  local o = setmetatable({}, Tree)
  o.depth = depth
  if depth > 0 then
    o.a = Tree.new(depth - 1)
    o.b = Tree.new(depth - 1)
    o.c = Tree.new(depth - 1)
    o.d = Tree.new(depth - 1)
    o.e = Tree.new(depth - 1)
  end
  return o
end

function Tree:walk()
  if self.depth == 0 then return 0 end
  return self.depth
      + self.a:walk()
      + self.b:walk()
      + self.c:walk()
      + self.d:walk()
      + self.e:walk()
end

local function main(tree, n)
  local last = 0
  for i = 1, n do
    last = tree:walk()
    if last ~= 122068 then print("Error") end
  end
  print(last)
end

local count = tonumber(arg[1])
local tree = Tree.new(8)

local t0 = os.clock()
main(tree, count)
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
