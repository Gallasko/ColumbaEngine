-- table_lookup.lua -- N inserts of (i -> i*2) into a table, then N
-- sequential lookups summing the values. Only the lookup loop is timed.

local count = tonumber(arg[1])
local d = {}
for i = 0, count - 1 do
    d[i] = i * 2
end

local t0 = os.clock()

local total = 0
for i = 0, count - 1 do
    total = total + d[i]
end

local elapsed_ns = (os.clock() - t0) * 1e9
print(math.floor(elapsed_ns))
