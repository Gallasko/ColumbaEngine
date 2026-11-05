-- clox/equality_count.lua -- clox's equality.lox made to count, in Lua: each
-- of its 19 comparisons adds one to a count when true, and the values are
-- locals swapped in pairs at the end of each pass (1 and 2, "str" and "stru",
-- true and false). The swaps keep every result: 4 comparisons are true each
-- pass. The whole loop is timed. arg[1] is the number of passes (clox:
-- 10000000). Prints the count, then the elapsed ns.

local function main(n)
  local one, two, nul = 1, 2, nil
  local str, stru = "str", "stru"
  local yes, no = true, false
  local count = 0

  for i = 1, n do
    if one == one then count = count + 1 end
    if one == two then count = count + 1 end
    if one == nul then count = count + 1 end
    if one == str then count = count + 1 end
    if one == yes then count = count + 1 end
    if nul == nul then count = count + 1 end
    if nul == one then count = count + 1 end
    if nul == str then count = count + 1 end
    if nul == yes then count = count + 1 end
    if yes == yes then count = count + 1 end
    if yes == one then count = count + 1 end
    if yes == no then count = count + 1 end
    if yes == str then count = count + 1 end
    if yes == nul then count = count + 1 end
    if str == str then count = count + 1 end
    if str == stru then count = count + 1 end
    if str == one then count = count + 1 end
    if str == nul then count = count + 1 end
    if str == yes then count = count + 1 end

    one, two = two, one
    str, stru = stru, str
    yes, no = no, yes
  end
  print(count)
end

local count = tonumber(arg[1])

local t0 = os.clock()
main(count)
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
