-- Training hooks (from data/training.zss). Options live on fighter:map().

local RoundState = { Intro = 0, Fight = 2 }
local DummyControl = { Cooperative = 0 }
local GuardMode = { None = 0, AutoAfterHit = 1, Always = 2 }
local Distance = { None = 0, Close = 1, Far = 3 }
local DummyMode = { None = 0, Crouch = 1, Jump = 2 }
local ButtonJam = { None = 0, A = 1 }

function TrainingUpdate(self, roundState, gameMode)
  if gameMode ~= "training" then return end
  if roundState == RoundState.Intro then
    self:powerSet(self:power())
    self:map("_iksys_trainingLifeTimer", 0)
    self:map("_iksys_trainingPowerTimer", 0)
  end
  if self:moveTypeH() then
    self:map("_iksys_trainingLifeTimer", 0)
  else
    self:map("_iksys_trainingLifeTimer", self:map("_iksys_trainingLifeTimer") + 1)
  end
  if self:map("_iksys_trainingLifeTimer") >= 60 then
    self:lifeSet(self:lifeMax())
    self:map("_iksys_trainingLifeTimer", 0)
  end
  if not self:ctrl() then
    self:map("_iksys_trainingPowerTimer", 0)
  else
    self:map("_iksys_trainingPowerTimer", self:map("_iksys_trainingPowerTimer") + 1)
  end
  if self:map("_iksys_trainingPowerTimer") >= 60 then
    self:powerSet(3000)
    self:map("_iksys_trainingPowerTimer", 0)
  end
  self:assertSpecial("noKo")

  if self:teamSide() ~= 2 then return end
  if roundState ~= RoundState.Fight then return end
  local control = self:map("_iksys_trainingDummyControl")
  if control ~= DummyControl.Cooperative then return end

  local guard = self:map("_iksys_trainingGuardMode")
  if guard == GuardMode.Always then
    self:assertSpecial("autoGuard")
  elseif guard == GuardMode.AutoAfterHit and self:moveTypeH() then
    self:assertSpecial("autoGuard")
  end

  local distMode = self:map("_iksys_trainingDistance")
  if distMode == Distance.Close then
    self:assertInput("F")
  elseif distMode == Distance.Far then
    self:assertInput("B")
  end

  local dummyMode = self:map("_iksys_trainingDummyMode")
  if dummyMode == DummyMode.Crouch then self:assertInput("D") end
  if dummyMode == DummyMode.Jump then self:assertInput("U") end

  local jam = self:map("_iksys_trainingButtonJam")
  if jam == ButtonJam.A then self:assertInput("a") end
end
