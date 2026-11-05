-- clox/equality.lua -- Crafting Interpreters' test/benchmark/equality.lox in
-- Lua: == between numbers, nil, booleans and strings, 19 a pass. Lua has no
-- expression statements, so each value goes to a dummy local `_` in both
-- loops. The first loop only moves the operands; the result is the second
-- loop's time minus the first's. arg[1] is the number of passes (clox:
-- 10000000). Prints the passes, then the equality ns.

local function main(n)
  local _
  local i = 0

  local loopStart = os.clock()
  while i < n do
    i = i + 1

    _ = 1; _ = 1; _ = 1; _ = 2; _ = 1; _ = nil; _ = 1; _ = "str"; _ = 1; _ = true
    _ = nil; _ = nil; _ = nil; _ = 1; _ = nil; _ = "str"; _ = nil; _ = true
    _ = true; _ = true; _ = true; _ = 1; _ = true; _ = false; _ = true; _ = "str"; _ = true; _ = nil
    _ = "str"; _ = "str"; _ = "str"; _ = "stru"; _ = "str"; _ = 1; _ = "str"; _ = nil; _ = "str"; _ = true
  end
  local loopTime = os.clock() - loopStart

  local start = os.clock()
  i = 0
  while i < n do
    i = i + 1

    _ = 1 == 1; _ = 1 == 2; _ = 1 == nil; _ = 1 == "str"; _ = 1 == true
    _ = nil == nil; _ = nil == 1; _ = nil == "str"; _ = nil == true
    _ = true == true; _ = true == 1; _ = true == false; _ = true == "str"; _ = true == nil
    _ = "str" == "str"; _ = "str" == "stru"; _ = "str" == 1; _ = "str" == nil; _ = "str" == true
  end
  local elapsed = os.clock() - start

  print(i)
  return (elapsed - loopTime) * 1e9
end

local count = tonumber(arg[1])
print(math.floor(main(count)))
