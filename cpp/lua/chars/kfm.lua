-- KFM Lua states (replaces CNS/ZSS). No upvalues across frames; all data on fighter userdata.

local function walk(self)
  if self:input("F") then
    self:velX(2.2)
    if self:state() ~= 20 then self:changeState(20) end
  elseif self:input("B") then
    self:velX(-1.6)
    if self:state() ~= 20 then self:changeState(20) end
  else
    self:velX(0)
    if self:ctrl() and self:state() ~= 0 then self:changeState(0) end
  end
end

function CharUpdate(self, p2)
  if self:moveTypeH() then
    return
  end
  if self:ctrl() then
    if self:command("a") or self:input("a") then
      self:changeState(200)
      self:velX(0)
      return
    end
    if self:input("D") then
      self:changeState(11)
      self:velX(0)
      return
    end
    walk(self)
  end
end
