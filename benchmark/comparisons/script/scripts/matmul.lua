-- matmul.lua -- naive O(n^3) square-matrix multiplication, flat 1-D storage.
-- arg[1] is the matrix size n. Init outside timed region.
--
-- Note: Lua tables are 1-indexed by convention, but to keep the index math
-- identical to .pg / .py we use 0-based explicit indices: A[i*n+k].

local function makeMatrix(n, offset)
    local m = {}
    for i = 0, n * n - 1 do
        m[i] = ((i + offset) % 17) * 1.0
    end
    return m
end

local n = tonumber(arg[1])
local A = makeMatrix(n, 0)
local B = makeMatrix(n, 7)
local C = {}
for i = 0, n * n - 1 do C[i] = 0.0 end

local t0 = os.clock()

for i = 0, n - 1 do
    for j = 0, n - 1 do
        local s = 0.0
        for k = 0, n - 1 do
            s = s + A[i * n + k] * B[k * n + j]
        end
        C[i * n + j] = s
    end
end

local elapsed_ns = (os.clock() - t0) * 1e9
print(math.floor(elapsed_ns))
