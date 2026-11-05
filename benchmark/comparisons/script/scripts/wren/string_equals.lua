-- wren/string_equals.lua -- Wren's test/benchmark/string_equals (Wren has a
-- Python version only; this follows it): ten string comparisons a pass,
-- equal and unequal, plus a string against a number. arg[1] is the number of
-- passes (Wren: 1000000). Prints how many were equal, then the elapsed ns.

local function main(n)
  local count = 0
  for i = 1, n do
    if "abc" == "abc" then count = count + 1 end
    if "a slightly longer string" ==
       "a slightly longer string" then count = count + 1 end
    if "a significantly longer string but still not overwhelmingly long string" ==
       "a significantly longer string but still not overwhelmingly long string" then count = count + 1 end

    if "" == "abc" then count = count + 1 end
    if "abc" == "abcd" then count = count + 1 end
    if "changed one character" == "changed !ne character" then count = count + 1 end
    if "123" == 123 then count = count + 1 end
    if "a slightly longer string" ==
       "a slightly longer string!" then count = count + 1 end
    if "a slightly longer string" ==
       "a slightly longer strinh" then count = count + 1 end
    if "a significantly longer string but still not overwhelmingly long string" ==
       "another" then count = count + 1 end
  end
  print(count)
end

local count = tonumber(arg[1])

local t0 = os.clock()
main(count)
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
