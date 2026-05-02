#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"
#include "fmod.hpp"

class USoundManager : public TSingleton<USoundManager>
{
    friend class TSingleton<USoundManager>;

public:
    void Init();
    void Update();
    void Release();

    bool LoadSound(const FString& SoundID, const FString& FilePath, bool bIsBGM);

    void PlayBGM(const FString& SoundID);
    void StopBGM();

    void PlaySFX(const FString& SoundID);
    void StopAllSFX();

    void SetMasterVolume(float Volume);
    void SetBGMVolume(float Volume);
    void SetSFXVolume(float Volume);

private:
    USoundManager() = default;
    ~USoundManager() = default;

private:
    FMOD::System* System = nullptr;

    FMOD::ChannelGroup* MasterGroup = nullptr;
    FMOD::ChannelGroup* BgmGroup = nullptr;
    FMOD::ChannelGroup* SfxGroup = nullptr;

    FMOD::Channel* CurrentBgmChannel = nullptr;

    TMap<FString, FMOD::Sound*> SoundCache;
};