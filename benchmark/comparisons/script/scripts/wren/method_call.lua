-- wren/method_call.lua -- Wren's test/benchmark/method_call.lua (old shootout
-- methcall, by Roberto Ierusalimschy): dynamic dispatch through a class and a
-- subclass, the loop body unrolled ten times. The classes are locals here.
-- arg[1] is n (Wren: 100000). Prints the two final values (true, false),
-- then the elapsed ns.

local Toggle = {}

function Toggle:value()
  return self.state
end

function Toggle:activate()
  self.state = not self.state
  return self
end

function Toggle:new(start_state)
  local o = {state = start_state}
  self.__index = self
  setmetatable(o, self)
  return o
end

local NthToggle = Toggle:new()

function NthToggle:activate()
  self.counter = self.counter + 1
  if self.counter >= self.count_max then
    Toggle.activate(self)
    self.counter = 0
  end
  return self
end

function NthToggle:new(start_state, max_counter)
  local o = Toggle.new(self, start_state)
  o.count_max = max_counter
  o.counter = 0
  return o
end

local function main(n)
  local val = true
  local toggle = Toggle:new(val)
  for i = 1, n do
    val = toggle:activate():value()
    val = toggle:activate():value()
    val = toggle:activate():value()
    val = toggle:activate():value()
    val = toggle:activate():value()
    val = toggle:activate():value()
    val = toggle:activate():value()
    val = toggle:activate():value()
    val = toggle:activate():value()
    val = toggle:activate():value()
  end
  print(toggle:value() and "true" or "false")

  val = true
  local ntoggle = NthToggle:new(val, 3)
  for i = 1, n do
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
    val = ntoggle:activate():value()
  end
  print(ntoggle:value() and "true" or "false")
end

local count = tonumber(arg[1])

local t0 = os.clock()
main(count)
local elapsed_ns = (os.clock() - t0) * 1e9

print(math.floor(elapsed_ns))
