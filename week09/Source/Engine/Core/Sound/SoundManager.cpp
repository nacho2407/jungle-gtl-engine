#include "SoundManager.h"

#include <filesystem>

#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"
#include "fmod_errors.h"

namespace
{
void LogFmodError(const char* Operation, FMOD_RESULT Result)
{
    if (Result != FMOD_OK)
    {
        UE_LOG(Sound, Warning, "%s failed. result=%d (%s)", Operation, static_cast<int>(Result), FMOD_ErrorString(Result));
    }
}
} // namespace

void FSoundManager::Init()
{
    if (bInitialized)
    {
        return;
    }

    FMOD_RESULT Result = FMOD::System_Create(&System);
    if (Result != FMOD_OK || !System)
    {
        UE_LOG(Sound, Error, "Failed to create FMOD system. result=%d (%s)", static_cast<int>(Result), FMOD_ErrorString(Result));
        System = nullptr;
        return;
    }

    Result = System->init(128, FMOD_INIT_NORMAL, nullptr);
    if (Result != FMOD_OK)
    {
        UE_LOG(Sound, Warning, "Failed to initialize FMOD output device. result=%d (%s)", static_cast<int>(Result), FMOD_ErrorString(Result));

        FMOD_RESULT FallbackResult = System->setOutput(FMOD_OUTPUTTYPE_NOSOUND);
        if (FallbackResult == FMOD_OK)
        {
            FallbackResult = System->init(128, FMOD_INIT_NORMAL, nullptr);
        }

        if (FallbackResult != FMOD_OK)
        {
            UE_LOG(Sound, Error, "Failed to initialize FMOD no-sound fallback. result=%d (%s)", static_cast<int>(FallbackResult), FMOD_ErrorString(FallbackResult));
            System->release();
            System = nullptr;
            return;
        }

        UE_LOG(Sound, Warning, "FMOD initialized with no-sound fallback.");
    }

    Result = System->getMasterChannelGroup(&MasterGroup);
    if (Result != FMOD_OK || !MasterGroup)
    {
        LogFmodError("FMOD getMasterChannelGroup", Result);
        System->close();
        System->release();
        System = nullptr;
        MasterGroup = nullptr;
        return;
    }

    Result = System->createChannelGroup("BGM", &BgmGroup);
    if (Result != FMOD_OK || !BgmGroup)
    {
        LogFmodError("FMOD createChannelGroup(BGM)", Result);
        System->close();
        System->release();
        System = nullptr;
        MasterGroup = nullptr;
        BgmGroup = nullptr;
        return;
    }

    Result = System->createChannelGroup("SFX", &SfxGroup);
    if (Result != FMOD_OK || !SfxGroup)
    {
        LogFmodError("FMOD createChannelGroup(SFX)", Result);
        BgmGroup->release();
        System->close();
        System->release();
        System = nullptr;
        MasterGroup = nullptr;
        BgmGroup = nullptr;
        SfxGroup = nullptr;
        return;
    }

    LogFmodError("FMOD addGroup(BGM)", MasterGroup->addGroup(BgmGroup));
    LogFmodError("FMOD addGroup(SFX)", MasterGroup->addGroup(SfxGroup));

    bInitialized = true;
    UE_LOG(Sound, Info, "SoundManager Initialized with FMOD.");
}

void FSoundManager::Tick()
{
    if (bInitialized && System)
    {
        LogFmodError("FMOD update", System->update());
    }
}

void FSoundManager::Release()
{
    if (!bInitialized && !System)
    {
        return;
    }

    bInitialized = false;

    if (MasterGroup)
    {
        LogFmodError("FMOD master stop", MasterGroup->stop());
    }

    CurrentBgmChannel = nullptr;

    for (auto& Pair : SoundCache)
    {
        if (Pair.second)
        {
            LogFmodError("FMOD sound release", Pair.second->release());
        }
    }
    SoundCache.clear();

    if (SfxGroup)
    {
        LogFmodError("FMOD SFX group release", SfxGroup->release());
    }
    if (BgmGroup)
    {
        LogFmodError("FMOD BGM group release", BgmGroup->release());
    }

    if (System)
    {
        LogFmodError("FMOD system close", System->close());
        LogFmodError("FMOD system release", System->release());
        System = nullptr;
    }

    MasterGroup = nullptr;
    BgmGroup = nullptr;
    SfxGroup = nullptr;
    CurrentBgmChannel = nullptr;
}

bool FSoundManager::LoadSound(const FString& SoundID, const FString& FilePath, bool bIsBGM)
{
    if (!bInitialized || !System)
    {
        UE_LOG(Sound, Warning, "LoadSound skipped because FMOD system is not initialized.");
        return false;
    }

    if (SoundCache.find(SoundID) != SoundCache.end())
    {
        return true;
    }

    FMOD::Sound* NewSound = nullptr;
    FMOD_MODE Mode = bIsBGM ? (FMOD_LOOP_NORMAL | FMOD_2D) : (FMOD_DEFAULT | FMOD_2D);

    FMOD_RESULT Result = System->createSound(FilePath.c_str(), Mode, nullptr, &NewSound);
    if (Result != FMOD_OK || !NewSound)
    {
        UE_LOG(Sound, Error, "Failed to load sound: %s. result=%d (%s)", FilePath.c_str(), static_cast<int>(Result), FMOD_ErrorString(Result));
        return false;
    }

    SoundCache[SoundID] = NewSound;
    return true;
}

void FSoundManager::LoadSoundsFromDirectory(const FString& RelativeDirPath, bool bIsBGM)
{
    if (!bInitialized || !System)
    {
        UE_LOG(Sound, Warning, "LoadSoundsFromDirectory skipped because FMOD system is not initialized.");
        return;
    }

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
    if (!bInitialized || !System || !BgmGroup)
    {
        return;
    }

    auto It = SoundCache.find(SoundID);
    if (It == SoundCache.end() || !It->second)
    {
        UE_LOG(Sound, Warning, "PlayBGM skipped because sound is not loaded: %s", SoundID.c_str());
        return;
    }

    StopBGM();

    FMOD_RESULT Result = System->playSound(It->second, BgmGroup, false, &CurrentBgmChannel);
    if (Result != FMOD_OK)
    {
        CurrentBgmChannel = nullptr;
        LogFmodError("FMOD play BGM", Result);
    }
}

void FSoundManager::StopBGM()
{
    if (!bInitialized || !CurrentBgmChannel)
    {
        CurrentBgmChannel = nullptr;
        return;
    }

    bool bIsPlaying = false;
    FMOD_RESULT Result = CurrentBgmChannel->isPlaying(&bIsPlaying);
    if (Result == FMOD_OK && bIsPlaying)
    {
        LogFmodError("FMOD stop BGM", CurrentBgmChannel->stop());
    }
    else if (Result != FMOD_OK)
    {
        LogFmodError("FMOD BGM isPlaying", Result);
    }

    CurrentBgmChannel = nullptr;
}

void FSoundManager::PlaySFX(const FString& SoundID)
{
    if (!bInitialized || !System || !SfxGroup)
    {
        return;
    }

    auto It = SoundCache.find(SoundID);
    if (It == SoundCache.end() || !It->second)
    {
        UE_LOG(Sound, Warning, "PlaySFX skipped because sound is not loaded: %s", SoundID.c_str());
        return;
    }

    FMOD::Channel* SFXChannel = nullptr;
    FMOD_RESULT Result = System->playSound(It->second, SfxGroup, false, &SFXChannel);

    if (Result != FMOD_OK)
    {
        UE_LOG(Sound, Warning, "Failed to play SFX: %s. result=%d (%s)", SoundID.c_str(), static_cast<int>(Result), FMOD_ErrorString(Result));
    }
}

void FSoundManager::StopAllSFX()
{
    if (bInitialized && SfxGroup)
    {
        LogFmodError("FMOD stop all SFX", SfxGroup->stop());
    }
}

void FSoundManager::SetMasterVolume(float Volume)
{
    if (bInitialized && MasterGroup)
    {
        LogFmodError("FMOD set master volume", MasterGroup->setVolume(Volume));
    }
}

void FSoundManager::SetBGMVolume(float Volume)
{
    if (bInitialized && BgmGroup)
    {
        LogFmodError("FMOD set BGM volume", BgmGroup->setVolume(Volume));
    }
}

void FSoundManager::SetSFXVolume(float Volume)
{
    if (bInitialized && SfxGroup)
    {
        LogFmodError("FMOD set SFX volume", SfxGroup->setVolume(Volume));
    }
}
