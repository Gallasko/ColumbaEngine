-- wren/binary_trees.lua -- Wren's test/benchmark/binary_trees. Wren's own Lua
-- file is Mike Pall's Benchmarks Game program, whose iteration counts and
-- items differ from the Wren/Python driver; this one keeps Pall's tree
-- functions and uses the Wren/Python driver so all versions do the same work.
-- arg[1] is maxDepth (Wren: 12). Prints each check, then the elapsed ns.

local function BottomUpTree(item, depth)
  if depth > 0 then
    local i = item + item
    depth = depth - 1
    local left, right = BottomUpTree(i - 1, depth), BottomUpTree(i, depth)
    return { item, left, right }
  else
    return { item }
  end
end

local function ItemCheck(tree)
  if tree[2] then
    return tree[1] + ItemCheck(tree[2]) - ItemCheck(tree[3])
  else
    return tree[1]
  end
end

local function main(maxDepth)
  local minDepth = 4
  local stretchDepth = maxDepth + 1

  print(ItemCheck(BottomUpTree(0, stretchDepth)))

  local longLivedTree = BottomUpTree(0, maxDepth)

  local iterations = 1
  for d = 1, maxDepth do iterations = iterations * 2 end

  local depth = minDepth
  while depth < stretchDepth do
    local check = 0
    for i = 1, iterations do
      check = check + ItemCheck(BottomUpTree(i, depth)) + ItemCheck(BottomUpTree(-i, depth))
    end
    print(check)
    iterations = iterations // 4
    depth = depth + 2
  end

  print(ItemCheck(longLivedTree))
end

local count = tonumber(arg[1])

local t0 = os.clock()
main(count)
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
