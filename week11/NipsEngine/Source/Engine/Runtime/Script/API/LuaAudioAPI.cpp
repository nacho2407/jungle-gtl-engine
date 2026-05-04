#include "Runtime/Script/API/LuaEngineAPIBindings.h"

#include "Audio/AudioSystem.h"
#include "Engine/Runtime/Engine.h"

#include <algorithm>

namespace FLuaEngineAPI
{
    void BindAudio(sol::state& Lua, sol::table& API)
    {
        sol::table Audio = Lua.create_table();

        Audio["PlayBGM"] = sol::overload(
            [](const FString& Path)
            {
                if (GEngine)
                {
                    GEngine->GetAudioSystem().PlayBGM(Path);
                }
            },
            [](const FString& Path, float FadeInSeconds)
            {
                if (GEngine)
                {
                    GEngine->GetAudioSystem().PlayBGM(Path, FadeInSeconds);
                }
            });

        Audio["StopBGM"] = sol::overload(
            []()
            {
                if (GEngine)
                {
                    GEngine->GetAudioSystem().StopBGM();
                }
            },
            [](float FadeOutSeconds)
            {
                if (GEngine)
                {
                    GEngine->GetAudioSystem().StopBGM(FadeOutSeconds);
                }
            });

        Audio["PlaySFX"] = sol::overload(
            [](const FString& Path) -> int32
            {
                if (GEngine)
                {
                    return static_cast<int32>(GEngine->GetAudioSystem().PlaySFX(Path));
                }
                return 0;
            },
            [](const FString& Path, float VolumeScale) -> int32
            {
                if (GEngine)
                {
                    return static_cast<int32>(GEngine->GetAudioSystem().PlaySFX(Path, VolumeScale));
                }
                return 0;
            });

        Audio["PlaySFX3D"] = sol::overload(
            [](const FString& Path, const FVector& Position) -> int32
            {
                if (GEngine)
                {
                    return static_cast<int32>(GEngine->GetAudioSystem().PlaySFX3D(Path, Position));
                }
                return 0;
            },
            [](const FString& Path, const FVector& Position, float VolumeScale) -> int32
            {
                if (GEngine)
                {
                    return static_cast<int32>(GEngine->GetAudioSystem().PlaySFX3D(Path, Position, VolumeScale));
                }
                return 0;
            });

        Audio["SetMasterVolume"] = [](float Volume)
        {
            if (GEngine)
            {
                GEngine->GetAudioSystem().SetMasterVolume(Volume);
            }
        };

        Audio["SetBGMVolume"] = [](float Volume)
        {
            if (GEngine)
            {
                GEngine->GetAudioSystem().SetBGMVolume(Volume);
            }
        };

        Audio["SetSFXVolume"] = [](float Volume)
        {
            if (GEngine)
            {
                GEngine->GetAudioSystem().SetSFXVolume(Volume);
            }
        };

        Audio["GetMasterVolume"] = []() -> float
        {
            return GEngine ? GEngine->GetAudioSystem().GetMasterVolume() : 1.0f;
        };

        Audio["GetBGMVolume"] = []() -> float
        {
            return GEngine ? GEngine->GetAudioSystem().GetBGMVolume() : 1.0f;
        };

        Audio["GetSFXVolume"] = []() -> float
        {
            return GEngine ? GEngine->GetAudioSystem().GetSFXVolume() : 1.0f;
        };

        Audio["SetListener"] = [](const FVector& Position, const FVector& Forward, const FVector& Up)
        {
            if (GEngine)
            {
                GEngine->GetAudioSystem().SetListener(Position, Forward, Up);
            }
        };

        Audio["SetPlaybackPolicy"] = [](const FString& Path, int MaxConcurrent, float CooldownSeconds, sol::optional<bool> bStopOldestWhenFull)
        {
            if (!GEngine)
            {
                return;
            }

            FAudioPlaybackPolicy Policy;
            Policy.MaxConcurrent = std::max(0, MaxConcurrent);
            Policy.CooldownSeconds = std::max(0.0f, CooldownSeconds);
            Policy.bStopOldestWhenFull = bStopOldestWhenFull.value_or(true);
            GEngine->GetAudioSystem().SetPlaybackPolicy(Path, Policy);
        };

        Audio["ClearPlaybackPolicy"] = [](const FString& Path)
        {
            if (GEngine)
            {
                GEngine->GetAudioSystem().ClearPlaybackPolicy(Path);
            }
        };

        Audio["StopSound"] = sol::overload(
            [](int32 Handle)
            {
                if (GEngine)
                {
                    GEngine->GetAudioSystem().StopSound(static_cast<FAudioHandle>(Handle));
                }
            },
            [](int32 Handle, float FadeOutSeconds)
            {
                if (GEngine)
                {
                    GEngine->GetAudioSystem().StopSound(static_cast<FAudioHandle>(Handle), FadeOutSeconds);
                }
            });

        Audio["IsSoundPlaying"] = [](int32 Handle) -> bool
        {
            return GEngine && GEngine->GetAudioSystem().IsHandleValid(static_cast<FAudioHandle>(Handle));
        };

        Audio["SetSoundPosition"] = [](int32 Handle, const FVector& Position)
        {
            if (GEngine)
            {
                GEngine->GetAudioSystem().SetSoundPosition(static_cast<FAudioHandle>(Handle), Position);
            }
        };

        Audio["RegisterSound"] = [](const FString& Key, const FString& Path)
        {
            if (GEngine)
            {
                GEngine->GetAudioSystem().RegisterSound(Key, Path);
            }
        };

        Audio["ResolveSoundPath"] = [](const FString& KeyOrPath) -> FString
        {
            return GEngine ? GEngine->GetAudioSystem().ResolveSoundPath(KeyOrPath) : FString{};
        };

        Audio["ReloadSoundRegistry"] = []()
        {
            if (GEngine)
            {
                GEngine->GetAudioSystem().ReloadSoundRegistry();
            }
        };

        Audio["StopAll"] = []()
        {
            if (GEngine)
            {
                GEngine->GetAudioSystem().StopAll();
            }
        };

        API["Audio"] = Audio;
    }
}
