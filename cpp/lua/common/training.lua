-- Training hooks (from data/training.zss). Options live on fighter:map().

function TrainingUpdate(self, roundState, gameMode)
  if gameMode ~= "training" then return end
  if roundState == 0 then
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
  if roundState ~= 2 then return end
  local control = self:map("_iksys_trainingDummyControl")
  if control ~= 0 then return end

  local guard = self:map("_iksys_trainingGuardMode")
  if guard == 2 then
    self:assertSpecial("autoGuard")
  elseif guard == 1 and self:moveTypeH() then
    self:assertSpecial("autoGuard")
  end

  local distMode = self:map("_iksys_trainingDistance")
  if distMode == 1 then
    self:assertInput("F")
  elseif distMode == 3 then
    self:assertInput("B")
  end

  local dummyMode = self:map("_iksys_trainingDummyMode")
  if dummyMode == 1 then self:assertInput("D") end
  if dummyMode == 2 then self:assertInput("U") end

  local jam = self:map("_iksys_trainingButtonJam")
  if jam == 1 then self:assertInput("a") end
end
