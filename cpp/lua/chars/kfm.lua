-- KFM character logic (option A). C++ owns physics, clsn, damage, KO flag, net.
-- Numbers match MUGEN/Ikemen common1; names are the enum.

local State = {
  Stand = 0,
  StandToCrouch = 10,
  Crouch = 11,
  CrouchToStand = 12,
  Walk = 20,
  JumpStart = 40,
  Jump = 50,
  Land = 52,
  Run = 100,
  RunBack = 105,
  RunBackLand = 106,
  GuardStart = 120,
  GuardStand = 130,
  GuardCrouch = 131,
  GuardHitStand = 150,
  GuardHitCrouch = 152,
  GuardHitAir = 154,
  StandingPunch = 200,
  KfmAirSpecial = 212,
  StandGetHitShake = 5000,
  StandGetHitSlide = 5001,
  CrouchGetHitShake = 5010,
  CrouchGetHitSlide = 5011,
  AirGetHitShake = 5020,
  AirGetHit = 5030,
  HitFall = 5050,
  HitBounce = 5100,
  LieDown = 5110,
  GetUp = 5120,
  LieDownKO = 5150,
}

local function gethit(self)
  local st = self:state()
  if st == State.StandGetHitShake or st == State.CrouchGetHitShake then
    if self:hitShakeOver() then
      if not self:alive() and self:posY() >= 0 then
        self:changeState(State.LieDownKO, 0)
      elseif st == State.CrouchGetHitShake then
        self:changeState(State.CrouchGetHitSlide, 0)
      else
        self:changeState(State.StandGetHitSlide, 0)
      end
    end
    return
  end
  if st == State.StandGetHitSlide or st == State.CrouchGetHitSlide then
    if self:time() == 0 then self:hitVelSet(true, false) end
    if self:hitOver() then
      self:velX(0)
      if not self:alive() then
        self:changeState(State.LieDownKO, 0)
      else
        local recover = (st == State.CrouchGetHitSlide) and State.Crouch or State.Stand
        self:changeState(recover, 1)
      end
    end
    return
  end
  if st == State.AirGetHit or st == State.HitFall then
    if self:posY() >= 0 then
      self:changeState(self:alive() and State.LieDown or State.LieDownKO, 0)
    end
    return
  end
  if st == State.LieDown then
    if not self:alive() then self:changeState(State.LieDownKO, 0) end
  end
end

local function loco(self)
  local st = self:state()
  if st == State.StandingPunch then
    if self:time() <= 1 then self:hitDef(40, -5) end
    if self:animEnded() or self:time() > 24 then self:changeState(State.Stand, 1) end
    return
  end
  if st == State.JumpStart then
    if self:animEnded() or self:time() >= 7 then
      local vx = 0
      if self:input("F") then vx = 2.5 elseif self:input("B") then vx = -2.55 end
      self:velX(vx)
      self:velY(-8.4)
      self:changeState(State.Jump, 1)
    end
    return
  end
  if st == State.Jump then
    if self:velY() > 0 and self:posY() >= 0 then self:changeState(State.Land, 0) end
    return
  end
  if st == State.Land then
    if self:animEnded() or self:time() > 12 then self:changeState(State.Stand, 1) end
    return
  end
  if st == State.StandToCrouch then
    if self:animEnded() or self:time() > 3 then self:changeState(State.Crouch, 1) end
    return
  end
  if st == State.Crouch then
    if not self:input("D") then self:changeState(State.CrouchToStand, 1) end
    return
  end
  if st == State.CrouchToStand then
    if self:animEnded() or self:time() > 4 then self:changeState(State.Stand, 1) end
    return
  end
  if not self:ctrl() then return end
  if self:command("a") or self:input("a") then
    self:changeState(State.StandingPunch, 0)
    self:hitDef(40, -5)
    self:velX(0)
    return
  end
  if self:input("U") then
    self:changeState(State.JumpStart, 0)
    return
  end
  if self:input("D") then
    self:changeState(State.StandToCrouch, 0)
    self:velX(0)
    return
  end
  if self:input("F") then
    self:velX(2.4)
    if st ~= State.Walk then self:changeState(State.Walk, 1) end
  elseif self:input("B") then
    self:velX(-2.2)
    if st ~= State.Walk then self:changeState(State.Walk, 1) end
  else
    self:velX(0)
    if st == State.Walk then self:changeState(State.Stand, 1) end
  end
end

function CharUpdate(self, p2)
  if not self:alive() then
    local st = self:state()
    if st == State.LieDownKO then return end
    if self:posY() >= 0 and st ~= State.StandGetHitShake and st ~= State.CrouchGetHitShake then
      self:changeState(State.LieDownKO, 0)
      return
    end
    if st == State.Stand or st == State.Walk or st == State.Crouch then
      self:changeState(State.HitFall, 0)
    end
    gethit(self)
    return
  end
  if self:moveTypeH() then
    gethit(self)
    return
  end
  loco(self)
end
