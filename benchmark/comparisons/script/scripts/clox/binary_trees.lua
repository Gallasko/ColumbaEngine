-- clox/binary_trees.lua -- Crafting Interpreters' test/benchmark/
-- binary_trees.lox in Lua: a Tree class that stores item and depth and sets
-- both children to nil at a leaf. arg[1] is maxDepth (clox: 14). Prints each
-- check value on its own line, then the elapsed ns.

local Tree = {}
Tree.__index = Tree

function Tree.new(item, depth)
  local o = setmetatable({}, Tree)
  o.item = item
  o.depth = depth
  if depth > 0 then
    local item2 = item + item
    depth = depth - 1
    o.left = Tree.new(item2 - 1, depth)
    o.right = Tree.new(item2, depth)
  else
    o.left = nil
    o.right = nil
  end
  return o
end

function Tree:check()
  if self.left == nil then
    return self.item
  end
  return self.item + self.left:check() - self.right:check()
end

local function main(maxDepth)
  local minDepth = 4
  local stretchDepth = maxDepth + 1

  print(Tree.new(0, stretchDepth):check())

  local longLivedTree = Tree.new(0, maxDepth)

  local iterations = 1
  local d = 0
  while d < maxDepth do
    iterations = iterations * 2
    d = d + 1
  end

  local depth = minDepth
  while depth < stretchDepth do
    local check = 0
    local i = 1
    while i <= iterations do
      check = check + Tree.new(i, depth):check() + Tree.new(-i, depth):check()
      i = i + 1
    end
    print(check)
    iterations = iterations // 4
    depth = depth + 2
  end

  print(longLivedTree:check())
end

local count = tonumber(arg[1])

local t0 = os.clock()
main(count)
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
