-- binary_tree.lua -- build a balanced binary tree of depth arg[1], then
-- recursively count its nodes. Same shape as binary_tree.pg / .py.

-- Same shape as the .pg / .py versions: each node stores `d` (depth) so
-- leaves can be detected via int==int. See binary_tree.py for the reasoning.

local function makeNode(depth)
    local n = { d = depth }
    if depth > 0 then
        n.left = makeNode(depth - 1)
        n.right = makeNode(depth - 1)
    end
    return n
end

local function countNodes(node)
    if node.d == 0 then return 1 end
    return 1 + countNodes(node.left) + countNodes(node.right)
end

local count = tonumber(arg[1])

local t0 = os.clock()
local root = makeNode(count)
local total = countNodes(root)
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
