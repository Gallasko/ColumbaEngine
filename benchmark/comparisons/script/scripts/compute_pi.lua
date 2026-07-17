-- compute_pi.lua -- Leibniz series approximation of pi.
-- Reads count from arg[1]; prints self-measured elapsed ns as last stdout line.
--
-- Lua 5.4 has no built-in nanosecond clock; os.clock() returns CPU seconds
-- with platform-dependent precision (microseconds on Linux). For our
-- pure-CPU workloads CPU time tracks wall time closely.

local function compute_pi(iterations)
    local pi = 0.0
    local i = 0
    while i < iterations do
        local term = 1.0 / (2 * i + 1)
        local even = i - (i // 2) * 2
        if even == 0 then
            pi = pi + term
        end
        if even == 1 then
            pi = pi - term
        end
        i = i + 1
    end
    return 4 * pi
end

local count = tonumber(arg[1])

local t0 = os.clock()
local result = compute_pi(count)
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
