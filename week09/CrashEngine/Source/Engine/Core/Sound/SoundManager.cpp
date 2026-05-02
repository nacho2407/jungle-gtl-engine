#include "SoundManager.h"

#include <filesystem>

#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"

void FSoundManager::Init()
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

void FSoundManager::Tick()
{
    if (System)
    {
        System->update();
    }
}

void FSoundManager::Release()
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

bool FSoundManager::LoadSound(const FString& SoundID, const FString& FilePath, bool bIsBGM)
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

void FSoundManager::LoadSoundsFromDirectory(const FString& RelativeDirPath, bool bIsBGM)
{
    std::wstring WideDirPath = FPaths::Combine(FPaths::ContentDir(), FPaths::ToWide(RelativeDirPath));

    if (!std::filesystem::exists(WideDirPath))
    {
        UE_LOG(Sound, Warning, "Directory does not exist: %s", RelativeDirPath.c_str());
        return;
    }

    for (const auto& Entry : std::filesystem::directory_iterator(WideDirPath))
    {
        if (Entry.is_regular_file())
        {
            std::string Extension = Entry.path().extension().string();

            if (Extension == ".wav" || Extension == ".mp3" || Extension == ".ogg")
            {
                std::string SoundID = Entry.path().stem().string();

                std::string FullFilePath = Entry.path().string();
                if (LoadSound(SoundID.c_str(), FullFilePath.c_str(), bIsBGM))
                {
                    UE_LOG(Sound, Info, "Auto-loaded sound: [%s]", SoundID.c_str());
                }
            }
        }
    }
}

void FSoundManager::PlayBGM(const FString& SoundID)
{
    if (SoundCache.find(SoundID) == SoundCache.end()) return;

    StopBGM(); 

    System->playSound(SoundCache[SoundID], BgmGroup, false, &CurrentBgmChannel);
}

void FSoundManager::StopBGM()
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

void FSoundManager::PlaySFX(const FString& SoundID)
{
    if (SoundCache.find(SoundID) == SoundCache.end()) return;

    System->playSound(SoundCache[SoundID], SfxGroup, false, nullptr);
}

void FSoundManager::StopAllSFX()
{
    if (SfxGroup)
    {
        SfxGroup->stop();
    }
}

void FSoundManager::SetMasterVolume(float Volume)
{
    if (MasterGroup)
    {
        MasterGroup->setVolume(Volume);
    }
}

void FSoundManager::SetBGMVolume(float Volume)
{
    if (BgmGroup) BgmGroup->setVolume(Volume);
}

void FSoundManager::SetSFXVolume(float Volume)
{
    if (SfxGroup) SfxGroup->setVolume(Volume);
}