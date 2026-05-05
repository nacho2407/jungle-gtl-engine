local SoundManager = {}
SoundManager.__index = SoundManager

function SoundManager.new(context)
    return setmetatable({
        context = context,

        BGM = {
            Main = "Asset/Audio/BGM/Main.mp3",
            Gameplay = "Asset/Audio/BGM/InGame.mp3",
            Clear = "Asset/Audio/BGM/Clear.mp3",
            Failed = "Asset/Audio/BGM/Failed.mp3",
            Title = "Asset/Audio/BGM/Main.mp3",
        },

        SFX = {
            Button = "Asset/Audio/SFX/UIButtonClick.mp3",
            IntroText = "Asset/Audio/SFX/UIIntroText.mp3",
            LoadingDone = "Asset/Audio/SFX/UILoadingDone.mp3",
            MainLogo = "Asset/Audio/SFX/UIMainLogo.mp3",
            Attack = {
                "Asset/Audio/SFX/GameAttack1.WAV",
                "Asset/Audio/SFX/GameAttack2.WAV",
                "Asset/Audio/SFX/GameAttack3.WAV",
                "Asset/Audio/SFX/GameAttack4.WAV"
            },
            AttackNone = "Asset/Audio/SFX/GameAttackNone.WAV",
            AttackGround = "Asset/Audio/SFX/GameAttackGround.WAV",
            PlayerDash = "Asset/Audio/SFX/GamePlayerDash.mp3",
            PlayerHit = "Asset/Audio/SFX/GamePlayerHit.mp3",
            PlayerRecover = "Asset/Audio/SFX/GamePlayerRecovery.mp3",
            TimeSlowStart = "Asset/Audio/SFX/GameTimeSlowStart.mp3",
            TimeSlowEnd = "Asset/Audio/SFX/GameTimeSlowEnd.mp3",
        },

        bgmVolume = Engine.API.Audio.GetBGMVolume(),
        sfxVolume = Engine.API.Audio.GetSFXVolume(),
        activeAttacks = {}
    }, SoundManager)
end

function SoundManager:BeginPlay()
    self.activeAttacks = {}

    self.context.eventBus:Subscribe("Player.AttackStarted", self, function(payload)
        self:BeginAttack(payload and payload.attackId)
    end)

    self.context.eventBus:Subscribe("Player.AttackHit", self, function(payload)
        self:MarkAttackHit(payload and payload.attackId)
    end)

    self.context.eventBus:Subscribe("Player.AttackFinished", self, function(payload)
        self:FinishAttack(payload and payload.attackId)
    end)

    self.context.eventBus:Subscribe("Player.AttackGround", self, function()
        self:PlayAttackGround()
    end)

    self.context.eventBus:Subscribe("Player.Dashed", self, function()
        self:PlayPlayerDash()
    end)

    self.context.eventBus:Subscribe("Player.Damaged", self, function()
        self:PlayPlayerHit()
    end)

    self.context.eventBus:Subscribe("Player.Recovered", self, function()
        self:PlayPlayerRecover()
    end)

    self.context.eventBus:Subscribe("TimeSlow.Started", self, function()
        self:PlayTimeSlowStart()
    end)

    self.context.eventBus:Subscribe("TimeSlow.Ended", self, function()
        self:PlayTimeSlowEnd()
    end)

    self.context.eventBus:Subscribe("Game.Started", self, function()
        self.activeAttacks = {}
    end)

    self.context.eventBus:Subscribe("Game.Finished", self, function()
        self.activeAttacks = {}
    end)

    self.context.eventBus:Subscribe("Game.Canceled", self, function()
        self.activeAttacks = {}
    end)
end

function SoundManager:PlayBGM(path, fadeIn)
    Engine.API.Debug.Log("BGM ON")
    if path == nil or path == "" then
        return
    end
    
    Engine.API.Audio.PlayBGM(path, fadeIn or 0.5)
end

function SoundManager:SetBGMVolume(volume)
    self.bgmVolume = math.max(0.0, math.min(1.0, volume or self.bgmVolume))
    Engine.API.Audio.SetBGMVolume(self.bgmVolume)
end

function SoundManager:SetSFXVolume(volume)
    self.sfxVolume = math.max(0.0, math.min(1.0, volume or self.sfxVolume))
    Engine.API.Audio.SetSFXVolume(self.sfxVolume)
end

function SoundManager:AdjustBGMVolume(delta)
    self:SetBGMVolume(self.bgmVolume + (delta or 0.0))
end

function SoundManager:AdjustSFXVolume(delta)
    self:SetSFXVolume(self.sfxVolume + (delta or 0.0))
end

function SoundManager:StopBGM(fadeOut)
    Engine.API.Audio.StopBGM(fadeOut or 0.5)
end

function SoundManager:PlaySFX(path, volumeScale)
    if path == nil or path == "" then
        return
    end

    Engine.API.Audio.PlaySFX(path, volumeScale or 1.0)
end

function SoundManager:PlayRandomAttack()
    local attacks = self.SFX.Attack
    if attacks == nil or #attacks <= 0 then
        return
    end

    local index = Engine.API.Random.RandomInt(1, #attacks)
    self:PlaySFX(attacks[index])
end

function SoundManager:PlayAttackNone()
    self:PlaySFX(self.SFX.AttackNone)
end

function SoundManager:PlayAttackGround()
    self:PlaySFX(self.SFX.AttackGround)
end

function SoundManager:PlayPlayerDash()
    self:PlaySFX(self.SFX.PlayerDash)
end

function SoundManager:PlayPlayerHit()
    self:PlaySFX(self.SFX.PlayerHit, 1.0)
end

function SoundManager:PlayPlayerRecover()
    self:PlaySFX(self.SFX.PlayerRecover)
end

function SoundManager:PlayTimeSlowStart()
    self:PlaySFX(self.SFX.TimeSlowStart)
end

function SoundManager:PlayTimeSlowEnd()
    self:PlaySFX(self.SFX.TimeSlowEnd)
end

function SoundManager:BeginAttack(attackId)
    if attackId == nil or attackId == "" then
        return
    end

    self.activeAttacks[attackId] = {
        hit = false
    }
end

function SoundManager:MarkAttackHit(attackId)
    if attackId == nil or attackId == "" then
        return
    end

    local attack = self.activeAttacks[attackId]
    if attack == nil then
        attack = { hit = false }
        self.activeAttacks[attackId] = attack
    end

    if attack.hit then
        return
    end

    attack.hit = true
    self:PlayRandomAttack()
end

function SoundManager:FinishAttack(attackId)
    if attackId == nil or attackId == "" then
        return
    end

    local attack = self.activeAttacks[attackId]
    if attack == nil then
        return
    end

    if not attack.hit then
        self:PlayAttackNone()
    end

    self.activeAttacks[attackId] = nil
end

function SoundManager:PlayButtonClick()
    self:PlaySFX(self.SFX.Button)
end

function SoundManager:PlayIntroText()
    self:PlaySFX(self.SFX.IntroText, 0.65)
end

function SoundManager:PlayLoadingDone()
    self:PlaySFX(self.SFX.LoadingDone)
end

function SoundManager:PlayMainLogo()
    self:PlaySFX(self.SFX.MainLogo)
end

function SoundManager:PlayUIAction(actionName)
    if actionName == "BGMVolumeChanged" or actionName == "SFXVolumeChanged" then
        return
    end

    self:PlayButtonClick()
end

function SoundManager:PlaySFX3D(path, position, volumeScale)
    if path == nil or path == "" or position == nil then
        return
    end

    Engine.API.Audio.PlaySFX3D(path, position, volumeScale or 1.0)
end

function SoundManager:EndPlay()
    self.context.eventBus:ClearOwner(self)
    self.activeAttacks = {}
    Engine.API.Audio.StopAll()
end

return SoundManager
