#include "SoundManager.h"
#include "Core/Logging/LogMacros.h"

void USoundManager::Init()
{
    FMOD::System_Create(&System);

    System->init(128, FMOD_INIT_NORMAL, nullptr);

    System->getMasterChannelGroup(&MasterGroup);
    System->createChannelGroup("BGM", &BgmGroup);
    System->createChannelGroup("SFX", &SfxGroup);

    MasterGroup->addGroup(BgmGroup);
    MasterGroup->addGroup(SfxGroup);

    UE_LOG(Sound, Info, "SoundManager Initialized with FMOD.");
}

void USoundManager::Update()
{
    if (System)
    {
        System->update();
    }
}

void USoundManager::Release()
{
    for (auto& Pair : SoundCache)
    {
        if (Pair.second)
        {
            Pair.second->release();
        }
    }
    SoundCache.clear();

    if (System)
    {
        System->close();
        System->release();
        System = nullptr;
    }
}

bool USoundManager::LoadSound(const FString& SoundID, const FString& FilePath, bool bIsBGM)
{
    if (SoundCache.find(SoundID) != SoundCache.end())
    {
        return true;
    }

    FMOD::Sound* NewSound = nullptr;

    FMOD_MODE Mode = bIsBGM ? (FMOD_LOOP_NORMAL | FMOD_2D) : (FMOD_DEFAULT | FMOD_2D);

    FMOD_RESULT Result = System->createSound(FilePath.c_str(), Mode, nullptr, &NewSound);
    if (Result != FMOD_OK)
    {
        UE_LOG(Sound, Error, "Failed to load sound: %s", FilePath.c_str());
        return false;
    }

    SoundCache[SoundID] = NewSound;
    return true;
}

void USoundManager::PlayBGM(const FString& SoundID)
{
    if (SoundCache.find(SoundID) == SoundCache.end()) return;

    StopBGM(); 

    System->playSound(SoundCache[SoundID], BgmGroup, false, &CurrentBgmChannel);
}

void USoundManager::StopBGM()
{
    if (CurrentBgmChannel)
    {
        bool bIsPlaying = false;
        CurrentBgmChannel->isPlaying(&bIsPlaying);
        if (bIsPlaying)
        {
            CurrentBgmChannel->stop();
        }
        CurrentBgmChannel = nullptr;
    }
}

void USoundManager::PlaySFX(const FString& SoundID)
{
    if (SoundCache.find(SoundID) == SoundCache.end()) return;

    System->playSound(SoundCache[SoundID], SfxGroup, false, nullptr);
}

void USoundManager::StopAllSFX()
{
    if (SfxGroup)
    {
        SfxGroup->stop();
    }
}

void USoundManager::SetMasterVolume(float Volume)
{
    if (MasterGroup)
    {
        MasterGroup->setVolume(Volume);
    }
}

void USoundManager::SetBGMVolume(float Volume)
{
    if (BgmGroup) BgmGroup->setVolume(Volume);
}

void USoundManager::SetSFXVolume(float Volume)
{
    if (SfxGroup) SfxGroup->setVolume(Volume);
}