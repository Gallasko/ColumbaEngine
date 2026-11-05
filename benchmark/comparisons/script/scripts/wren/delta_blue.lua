-- delta_blue.lua -- the DeltaBlue constraint solver, run `count` times
-- (arg[1]; Wren's suite uses 40), each a chain_test(100) and a
-- projection_test(100). Ported from Wren's test/benchmark/delta_blue.py (from
-- V8's deltablue.js, itself from the Smalltalk DeltaBlue by John Maloney and
-- Mario Wolczko), quirks kept so the `total` matches; classes are metatables
-- with __index inheritance. Lists are Lua arrays, `pop(0)` is table.remove(t, 1).

local planner = nil
local total = 0

-- Python's int(): truncation toward zero (values may be floats after `/`).
local function toint(x)
    local i = math.tointeger(x)
    if i then return i end
    if x >= 0 then return math.floor(x) end
    return math.ceil(x)
end

-- Python's list.remove: drop the first element equal to `x`.
local function listRemove(t, x)
    for i = 1, #t do
        if t[i] == x then
            table.remove(t, i)
            return
        end
    end
    error("list.remove(x): x not in list")
end

local function class(parent)
    local c = {}
    c.__index = c
    if parent then setmetatable(c, { __index = parent }) end
    return c
end

--- Strength

local Strength = class()

local function newStrength(strength, name)
    return setmetatable({ strength = strength, name = name }, Strength)
end

local REQUIRED = newStrength(0, "required")
local STRONG_PREFERRED = newStrength(1, "strongPreferred")
local PREFERRED = newStrength(2, "preferred")
local STRONG_DEFAULT = newStrength(3, "strongDefault")
local NORMAL = newStrength(4, "normal")
local WEAK_DEFAULT = newStrength(5, "weakDefault")
local WEAKEST = newStrength(6, "weakest")

local function stronger(s1, s2) return s1.strength < s2.strength end
local function weaker(s1, s2) return s1.strength > s2.strength end
local function weakestOf(s1, s2)
    if weaker(s1, s2) then return s1 end
    return s2
end

-- The Python version's (bugged, kept) map: index is self.strength.
local NEXT_WEAKER = {
    [0] = WEAKEST, [1] = WEAK_DEFAULT, [2] = NORMAL,
    [3] = STRONG_DEFAULT, [4] = PREFERRED, [5] = REQUIRED,
}

function Strength:nextWeaker()
    local s = NEXT_WEAKER[self.strength]
    if s == nil then error("KeyError: " .. self.strength) end
    return s
end

--- Direction

local NONE = 0
local FORWARD = 1
local BACKWARD = -1

--- Constraint

local Constraint = class()

function Constraint:addConstraint()
    self:addToGraph()
    planner:incrementalAdd(self)
end

function Constraint:satisfy(mark)
    self:chooseMethod(mark)

    if not self:isSatisfied() then
        if self.strength == REQUIRED then
            print("Could not satisfy a required constraint!")
        end
        return nil
    end

    self:markInputs(mark)
    local out = self:output()
    local overridden = out.determinedBy

    if overridden ~= nil then
        overridden:markUnsatisfied()
    end

    out.determinedBy = self

    if not planner:addPropagate(self, mark) then
        print("Cycle encountered")
    end

    out.mark = mark
    return overridden
end

function Constraint:destroyConstraint()
    if self:isSatisfied() then
        planner:incrementalRemove(self)
    else
        self:removeFromGraph()
    end
end

function Constraint:isInput() return false end

--- UrnaryConstraint

local UrnaryConstraint = class(Constraint)

local function initUrnary(self, v, strength)
    self.strength = strength
    self.myOutput = v
    self.satisfied = false
    self:addConstraint()
    return self
end

function UrnaryConstraint:addToGraph()
    self.myOutput:addConstraint(self)
    self.satisfied = false
end

function UrnaryConstraint:chooseMethod(mark)
    if self.myOutput.mark ~= mark and stronger(self.strength, self.myOutput.walkStrength) then
        self.satisfied = true
    else
        self.satisfied = false
    end
end

function UrnaryConstraint:isSatisfied() return self.satisfied end

function UrnaryConstraint:markInputs(mark) end

function UrnaryConstraint:output() return self.myOutput end

function UrnaryConstraint:recalculate()
    self.myOutput.walkStrength = self.strength
    self.myOutput.stay = not self:isInput()

    if self.myOutput.stay then
        self:execute()
    end
end

function UrnaryConstraint:markUnsatisfied() self.satisfied = false end

function UrnaryConstraint:inputsKnown(mark) return true end

function UrnaryConstraint:removeFromGraph()
    if self.myOutput ~= nil then
        self.myOutput:removeConstraint(self)
        self.satisfied = false
    end
end

local StayConstraint = class(UrnaryConstraint)

local function newStayConstraint(v, strength)
    return initUrnary(setmetatable({}, StayConstraint), v, strength)
end

function StayConstraint:execute() end

local EditConstraint = class(UrnaryConstraint)

local function newEditConstraint(v, strength)
    return initUrnary(setmetatable({}, EditConstraint), v, strength)
end

function EditConstraint:isInput() return true end

function EditConstraint:execute() end

--- BinaryConstraint

local BinaryConstraint = class(Constraint)

local function initBinary(self, v1, v2, strength)
    self.strength = strength
    self.v1 = v1
    self.v2 = v2
    self.direction = NONE
    self:addConstraint()
    return self
end

function BinaryConstraint:chooseMethod(mark)
    if self.v1.mark == mark then
        if self.v2.mark ~= mark and stronger(self.strength, self.v2.walkStrength) then
            self.direction = FORWARD
        else
            self.direction = BACKWARD
        end
    end

    if self.v2.mark == mark then
        if self.v1.mark ~= mark and stronger(self.strength, self.v1.walkStrength) then
            self.direction = BACKWARD
        else
            self.direction = NONE
        end
    end

    if weaker(self.v1.walkStrength, self.v2.walkStrength) then
        if stronger(self.strength, self.v1.walkStrength) then
            self.direction = BACKWARD
        else
            self.direction = NONE
        end
    else
        if stronger(self.strength, self.v2.walkStrength) then
            self.direction = FORWARD
        else
            self.direction = BACKWARD
        end
    end
end

function BinaryConstraint:addToGraph()
    self.v1:addConstraint(self)
    self.v2:addConstraint(self)
    self.direction = NONE
end

function BinaryConstraint:isSatisfied() return self.direction ~= NONE end

function BinaryConstraint:markInputs(mark)
    self:input().mark = mark
end

function BinaryConstraint:input()
    if self.direction == FORWARD then return self.v1 end
    return self.v2
end

function BinaryConstraint:output()
    if self.direction == FORWARD then return self.v2 end
    return self.v1
end

function BinaryConstraint:recalculate()
    local ihn = self:input()
    local out = self:output()
    out.walkStrength = weakestOf(self.strength, ihn.walkStrength)
    out.stay = ihn.stay

    if out.stay then
        self:execute()
    end
end

function BinaryConstraint:markUnsatisfied() self.direction = NONE end

function BinaryConstraint:inputsKnown(mark)
    local i = self:input()
    return i.mark == mark or i.stay or i.determinedBy == nil
end

function BinaryConstraint:removeFromGraph()
    if self.v1 ~= nil then
        self.v1:removeConstraint(self)
    end

    if self.v2 ~= nil then
        self.v2:removeConstraint(self)
    end

    self.direction = NONE
end

--- ScaleConstraint

local ScaleConstraint = class(BinaryConstraint)

local function newScaleConstraint(src, scale, offset, dest, strength)
    local self = setmetatable({}, ScaleConstraint)
    self.direction = NONE
    self.scale = scale
    self.offset = offset
    return initBinary(self, src, dest, strength)
end

function ScaleConstraint:addToGraph()
    BinaryConstraint.addToGraph(self)
    self.scale:addConstraint(self)
    self.offset:addConstraint(self)
end

function ScaleConstraint:removeFromGraph()
    BinaryConstraint.removeFromGraph(self)

    if self.scale ~= nil then
        self.scale:removeConstraint(self)
    end

    if self.offset ~= nil then
        self.offset:removeConstraint(self)
    end
end

function ScaleConstraint:markInputs(mark)
    BinaryConstraint.markInputs(self, mark)
    self.scale.mark = mark
    self.offset.mark = mark
end

function ScaleConstraint:execute()
    if self.direction == FORWARD then
        self.v2.value = self.v1.value * self.scale.value + self.offset.value
    else
        self.v1.value = (self.v2.value - self.offset.value) / self.scale.value
    end
end

function ScaleConstraint:recalculate()
    local ihn = self:input()
    local out = self:output()
    out.walkStrength = weakestOf(self.strength, ihn.walkStrength)
    out.stay = ihn.stay and self.scale.stay and self.offset.stay

    if out.stay then
        self:execute()
    end
end

--- EqualityConstraint

local EqualityConstraint = class(BinaryConstraint)

local function newEqualityConstraint(v1, v2, strength)
    return initBinary(setmetatable({}, EqualityConstraint), v1, v2, strength)
end

function EqualityConstraint:execute()
    self:output().value = self:input().value
end

--- Variable

local Variable = class()

local function newVariable(name, initialValue)
    return setmetatable({
        name = name,
        value = initialValue or 0,
        constraints = {},
        determinedBy = nil,
        mark = 0,
        walkStrength = WEAKEST,
        stay = true,
    }, Variable)
end

function Variable:addConstraint(constraint)
    local cs = self.constraints
    cs[#cs + 1] = constraint
end

function Variable:removeConstraint(constraint)
    listRemove(self.constraints, constraint)

    if self.determinedBy == constraint then
        self.determinedBy = nil
    end
end

--- Plan

local Plan = class()

local function newPlan()
    return setmetatable({ v = {} }, Plan)
end

function Plan:addConstraint(c)
    local v = self.v
    v[#v + 1] = c
end

function Plan:execute()
    local v = self.v
    for i = 1, #v do
        v[i]:execute()
    end
end

--- Planner

local Planner = class()

local function newPlanner()
    return setmetatable({ currentMark = 0 }, Planner)
end

function Planner:incrementalAdd(constraint)
    local mark = self:newMark()
    local overridden = constraint:satisfy(mark)

    while overridden ~= nil do
        overridden = overridden:satisfy(mark)
    end
end

function Planner:incrementalRemove(constraint)
    local out = constraint:output()
    constraint:markUnsatisfied()
    constraint:removeFromGraph()
    local unsatisfied = self:removePropagateFrom(out)
    local strength = REQUIRED
    -- Do-while, with next_weaker inside the for as in the Python version.
    local rep = true

    while rep do
        for i = 1, #unsatisfied do
            local u = unsatisfied[i]
            if u.strength == strength then
                self:incrementalAdd(u)
            end

            strength = strength:nextWeaker()
        end

        rep = strength ~= WEAKEST
    end
end

function Planner:newMark()
    self.currentMark = self.currentMark + 1
    return self.currentMark
end

function Planner:makePlan(sources)
    local mark = self:newMark()
    local plan = newPlan()
    local todo = sources

    while #todo > 0 do
        local c = table.remove(todo, 1)

        if c:output().mark ~= mark and c:inputsKnown(mark) then
            plan:addConstraint(c)
            c:output().mark = mark
            self:addConstraintsConsumingTo(c:output(), todo)
        end
    end

    return plan
end

function Planner:extractPlanFromConstraints(constraints)
    local sources = {}

    for i = 1, #constraints do
        local c = constraints[i]
        if c:isInput() and c:isSatisfied() then
            sources[#sources + 1] = c
        end
    end

    return self:makePlan(sources)
end

function Planner:addPropagate(c, mark)
    local todo = { c }

    while #todo > 0 do
        local d = table.remove(todo, 1)

        if d:output().mark == mark then
            self:incrementalRemove(c)
            return false
        end

        d:recalculate()
        self:addConstraintsConsumingTo(d:output(), todo)
    end

    return true
end

function Planner:removePropagateFrom(out)
    out.determinedBy = nil
    out.walkStrength = WEAKEST
    out.stay = true
    local unsatisfied = {}
    local todo = { out }

    while #todo > 0 do
        local v = table.remove(todo, 1)
        local cs = v.constraints

        for i = 1, #cs do
            local c = cs[i]
            if not c:isSatisfied() then
                unsatisfied[#unsatisfied + 1] = c
            end
        end

        local determining = v.determinedBy

        for i = 1, #cs do
            local c = cs[i]
            if c ~= determining and c:isSatisfied() then
                c:recalculate()
                todo[#todo + 1] = c:output()
            end
        end
    end

    return unsatisfied
end

function Planner:addConstraintsConsumingTo(v, coll)
    local determining = v.determinedBy
    local cc = v.constraints

    for i = 1, #cc do
        local c = cc[i]
        if c ~= determining and c:isSatisfied() then
            coll[#coll + 1] = c
        end
    end
end

--- Main

local function chainTest(n)
    planner = newPlanner()
    local prev, first, last = nil, nil, nil

    -- We need to go up to n inclusively.
    for i = 0, n do
        local name = "v" .. i
        local v = newVariable(name)

        if prev ~= nil then
            newEqualityConstraint(prev, v, REQUIRED)
        end

        if i == 0 then first = v end
        if i == n then last = v end

        prev = v
    end

    newStayConstraint(last, STRONG_DEFAULT)
    local edit = newEditConstraint(first, PREFERRED)
    local edits = { edit }
    local plan = planner:extractPlanFromConstraints(edits)

    for i = 0, 99 do
        first.value = i
        plan:execute()

        total = total + toint(last.value)
        if last.value ~= i then
            print("Chain test failed.")
        end
    end
end

local function change(v, newValue)
    local edit = newEditConstraint(v, PREFERRED)
    local edits = { edit }

    local plan = planner:extractPlanFromConstraints(edits)

    for i = 0, 9 do
        v.value = newValue
        plan:execute()
    end

    edit:destroyConstraint()
end

local function projectionTest(n)
    planner = newPlanner()
    local scale = newVariable("scale", 10)
    local offset = newVariable("offset", 1000)
    local src, dst = nil, nil

    local dests = {}

    for i = 0, n - 1 do
        src = newVariable("src" .. i, i)
        dst = newVariable("dst" .. i, i)
        dests[#dests + 1] = dst
        newStayConstraint(src, NORMAL)
        newScaleConstraint(src, scale, offset, dst, REQUIRED)
    end

    change(src, 17)

    total = total + toint(dst.value)
    if dst.value ~= 1170 then
        print("Projection 1 failed")
    end

    change(dst, 1050)

    total = total + toint(src.value)
    if src.value ~= 5 then
        print("Projection 2 failed")
    end

    change(scale, 5)

    for i = 0, n - 2 do
        total = total + toint(dests[i + 1].value)
        if dests[i + 1].value ~= (i * 5 + 1000) then
            print("Projection 3 failed")
        end
    end

    change(offset, 2000)

    for i = 0, n - 2 do
        total = total + toint(dests[i + 1].value)
        if dests[i + 1].value ~= (i * 5 + 2000) then
            print("Projection 4 failed")
        end
    end
end

local function main()
    local count = tonumber(arg[1])
    local t0 = os.clock()
    for i = 1, count do
        chainTest(100)
        projectionTest(100)
    end
    local elapsed = os.clock() - t0
    print(total)
    print(math.floor(elapsed * 1e9))
end

main()
