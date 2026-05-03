local SoundManager = {}
SoundManager.__index = SoundManager

function SoundManager.new(context)
    return setmetatable({
        context = context,

        BGM = {
            Title = "Asset/Audio/BGM/Title.wav",
            Gameplay = "Asset/Audio/BGM/Gameplay.wav",
            Result = "Asset/Audio/BGM/Result.wav"
        },

        SFX = {
            Button = "Asset/Audio/SFX/Button.wav",
            Slash = "Asset/Audio/SFX/Slash.wav",
            EnemyHit = "Asset/Audio/SFX/EnemyHit.wav"
        }
    }, SoundManager)
end

function SoundManager:PlayBGM(path, fadeIn)
    if path == nil or path == "" then
        return
    end

    Engine.API.Audio.PlayBGM(path, fadeIn or 0.5)
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
