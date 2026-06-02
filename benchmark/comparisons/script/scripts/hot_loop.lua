-- hot_loop.lua -- tight integer loop summing 1..count.

local count = tonumber(arg[1])

local t0 = os.clock()

local sum = 0
local i = 1
while i <= count do
    sum = sum + i
    i = i + 1
end

local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
