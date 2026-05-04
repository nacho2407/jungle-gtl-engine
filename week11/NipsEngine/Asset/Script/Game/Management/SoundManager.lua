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
        },

        bgmVolume = Engine.API.Audio.GetBGMVolume(),
        sfxVolume = Engine.API.Audio.GetSFXVolume()
    }, SoundManager)
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
    Engine.API.Audio.StopAll()
end

return SoundManager
