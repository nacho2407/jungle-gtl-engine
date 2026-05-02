#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "fmod.hpp"

class FSoundManager : public TSingleton<FSoundManager>
{
    friend class TSingleton<FSoundManager>;

public:
    void Init();
    void Tick();
    void Release();

    bool LoadSound(const FString& SoundID, const FString& FilePath, bool bIsBGM);
    void LoadSoundsFromDirectory(const FString& RelativeDirPath, bool bIsBGM);

    void PlayBGM(const FString& SoundID);
    void StopBGM();

    void PlaySFX(const FString& SoundID);
    void StopAllSFX();

    void SetMasterVolume(float Volume);
    void SetBGMVolume(float Volume);
    void SetSFXVolume(float Volume);

private:
    FSoundManager() = default;
    ~FSoundManager() = default;

private:
    FMOD::System* System = nullptr;

    FMOD::ChannelGroup* MasterGroup = nullptr;
    FMOD::ChannelGroup* BgmGroup = nullptr;
    FMOD::ChannelGroup* SfxGroup = nullptr;

    FMOD::Channel* CurrentBgmChannel = nullptr;

    TMap<FString, FMOD::Sound*> SoundCache;
};