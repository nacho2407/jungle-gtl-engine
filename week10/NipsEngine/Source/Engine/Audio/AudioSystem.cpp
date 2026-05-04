#include "Audio/AudioSystem.h"

#include "Core/Logging/Log.h"
#include "Core/Paths.h"

#include "SoLoud/include/soloud.h"
#include "SoLoud/include/soloud_wav.h"
#include "SoLoud/include/soloud_wavstream.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>

namespace
{
    float ClampVolume(float Volume)
    {
        return std::clamp(Volume, 0.0f, 1.0f);
    }

    FString NormalizeAudioPath(const FString& Path)
    {
        if (Path.empty())
        {
            return {};
        }

        return FPaths::Normalize(FPaths::ToAbsoluteString(FPaths::ToWide(Path)));
    }

    FString TrimAudioLine(const FString& Value)
    {
        const auto IsSpace = [](unsigned char Ch) { return std::isspace(Ch) != 0; };
        auto Begin = std::find_if_not(Value.begin(), Value.end(), IsSpace);
        auto End = std::find_if_not(Value.rbegin(), Value.rend(), IsSpace).base();
        if (Begin >= End)
        {
            return {};
        }
        return FString(Begin, End);
    }

    bool LooksLikePath(const FString& Value)
    {
        return Value.find('/') != FString::npos ||
               Value.find('\\') != FString::npos ||
               Value.find('.') != FString::npos ||
               Value.find(':') != FString::npos;
    }
}

struct FAudioSystem::FImpl
{
    SoLoud::Soloud Engine;
    TMap<FString, std::unique_ptr<SoLoud::Wav>> SFXCache;
    TMap<FString, std::unique_ptr<SoLoud::WavStream>> StreamCache;
    TMap<FString, FString> SoundRegistry;
    TMap<FString, FAudioPlaybackPolicy> PlaybackPolicies;
    TMap<FString, TArray<SoLoud::handle>> ActiveSFXHandles;
    TMap<FString, float> CooldownRemaining;

    SoLoud::handle BGMHandle = 0;
    FString CurrentBGMPath;
    float MasterVolume = 1.0f;
    float BGMVolume = 0.8f;
    float SFXVolume = 1.0f;
    bool bInitialized = false;
    bool bListenerDirty = false;

    FString ResolveSoundPath(const FString& KeyOrPath) const
    {
        if (KeyOrPath.empty())
        {
            return {};
        }

        if (auto It = SoundRegistry.find(KeyOrPath); It != SoundRegistry.end())
        {
            return NormalizeAudioPath(It->second);
        }

        if (LooksLikePath(KeyOrPath))
        {
            return NormalizeAudioPath(KeyOrPath);
        }

        static const char* Extensions[] = { ".wav", ".ogg", ".mp3", ".flac" };
        for (const char* Extension : Extensions)
        {
            const FString Candidate = "Asset/Audio/" + KeyOrPath + Extension;
            const FString NormalizedCandidate = NormalizeAudioPath(Candidate);
            if (std::filesystem::exists(FPaths::ToWide(NormalizedCandidate)))
            {
                return NormalizedCandidate;
            }
        }

        return NormalizeAudioPath(KeyOrPath);
    }

    SoLoud::Wav* LoadSFX(const FString& Path, bool bLoop)
    {
        const FString NormalizedPath = ResolveSoundPath(Path);
        if (NormalizedPath.empty())
        {
            return nullptr;
        }

        const FString CacheKey = NormalizedPath + (bLoop ? "#loop" : "#oneshot");
        if (auto It = SFXCache.find(CacheKey); It != SFXCache.end())
        {
            return It->second.get();
        }

        if (!std::filesystem::exists(FPaths::ToWide(NormalizedPath)))
        {
            UE_LOG_WARNING("[AudioSystem] SFX file not found: %s", NormalizedPath.c_str());
            return nullptr;
        }

        auto Clip = std::make_unique<SoLoud::Wav>();
        const SoLoud::result Result = Clip->load(NormalizedPath.c_str());
        if (Result != SoLoud::SO_NO_ERROR)
        {
            UE_LOG_WARNING("[AudioSystem] Failed to load SFX: %s (%s)", NormalizedPath.c_str(), Engine.getErrorString(Result));
            return nullptr;
        }

        Clip->setLooping(bLoop);
        SoLoud::Wav* LoadedClip = Clip.get();
        SFXCache.emplace(CacheKey, std::move(Clip));
        return LoadedClip;
    }

    SoLoud::WavStream* LoadStream(const FString& Path)
    {
        const FString NormalizedPath = ResolveSoundPath(Path);
        if (NormalizedPath.empty())
        {
            return nullptr;
        }

        if (auto It = StreamCache.find(NormalizedPath); It != StreamCache.end())
        {
            return It->second.get();
        }

        if (!std::filesystem::exists(FPaths::ToWide(NormalizedPath)))
        {
            UE_LOG_WARNING("[AudioSystem] Stream file not found: %s", NormalizedPath.c_str());
            return nullptr;
        }

        auto Stream = std::make_unique<SoLoud::WavStream>();
        const SoLoud::result Result = Stream->load(NormalizedPath.c_str());
        if (Result != SoLoud::SO_NO_ERROR)
        {
            UE_LOG_WARNING("[AudioSystem] Failed to load stream: %s (%s)", NormalizedPath.c_str(), Engine.getErrorString(Result));
            return nullptr;
        }

        Stream->setLooping(true);
        SoLoud::WavStream* LoadedStream = Stream.get();
        StreamCache.emplace(NormalizedPath, std::move(Stream));
        return LoadedStream;
    }

    bool CanPlaySFX(const FString& NormalizedPath)
    {
        auto PolicyIt = PlaybackPolicies.find(NormalizedPath);
        if (PolicyIt == PlaybackPolicies.end())
        {
            return true;
        }

        const FAudioPlaybackPolicy& Policy = PolicyIt->second;
        if (Policy.CooldownSeconds > 0.0f)
        {
            if (auto CooldownIt = CooldownRemaining.find(NormalizedPath);
                CooldownIt != CooldownRemaining.end() && CooldownIt->second > 0.0f)
            {
                return false;
            }
        }

        if (Policy.MaxConcurrent > 0)
        {
            TArray<SoLoud::handle>& Handles = ActiveSFXHandles[NormalizedPath];
            Handles.erase(
                std::remove_if(
                    Handles.begin(),
                    Handles.end(),
                    [this](SoLoud::handle Handle)
                    {
                        return !Engine.isValidVoiceHandle(Handle);
                    }),
                Handles.end());

            if (static_cast<int>(Handles.size()) >= Policy.MaxConcurrent)
            {
                if (Policy.bStopOldestWhenFull && !Handles.empty())
                {
                    Engine.stop(Handles.front());
                    Handles.erase(Handles.begin());
                }
                else
                {
                    return false;
                }
            }
        }

        return true;
    }

    void MarkSFXPlayed(const FString& NormalizedPath, SoLoud::handle Handle)
    {
        if (Handle == 0)
        {
            return;
        }

        ActiveSFXHandles[NormalizedPath].push_back(Handle);
        if (auto PolicyIt = PlaybackPolicies.find(NormalizedPath);
            PolicyIt != PlaybackPolicies.end() && PolicyIt->second.CooldownSeconds > 0.0f)
        {
            CooldownRemaining[NormalizedPath] = PolicyIt->second.CooldownSeconds;
        }
    }
};

FAudioSystem::FAudioSystem()
    : Impl(std::make_unique<FImpl>())
{
}

FAudioSystem::~FAudioSystem()
{
    Shutdown();
}

bool FAudioSystem::Initialize()
{
    if (Impl->bInitialized)
    {
        return true;
    }

    const unsigned int Flags = SoLoud::Soloud::CLIP_ROUNDOFF | SoLoud::Soloud::LEFT_HANDED_3D;
    struct FAudioInitAttempt
    {
        unsigned int SampleRate;
        unsigned int BufferSize;
        unsigned int Channels;
        const char* Label;
    };

    constexpr FAudioInitAttempt Attempts[] = {
        { SoLoud::Soloud::AUTO, SoLoud::Soloud::AUTO, 2, "default" },
        { 48000, 2048, 2, "48k/2048/stereo" },
        { 44100, 2048, 2, "44.1k/2048/stereo" },
        { 48000, 4096, 2, "48k/4096/stereo" },
        { 48000, 2048, 1, "48k/2048/mono" },
    };

    for (const FAudioInitAttempt& Attempt : Attempts)
    {
        const SoLoud::result Result = Impl->Engine.init(
            Flags,
            SoLoud::Soloud::MINIAUDIO,
            Attempt.SampleRate,
            Attempt.BufferSize,
            Attempt.Channels);
        if (Result == SoLoud::SO_NO_ERROR)
        {
            Impl->Engine.setMaxActiveVoiceCount(64);
            Impl->Engine.setGlobalVolume(Impl->MasterVolume);
            Impl->bInitialized = true;
            ReloadSoundRegistry();
            UE_LOG("[AudioSystem] Initialized. Backend=%s Attempt=%s", Impl->Engine.getBackendString(), Attempt.Label);
            return true;
        }

        UE_LOG_WARNING("[AudioSystem] MiniAudio init failed. Attempt=%s Error=%s", Attempt.Label, Impl->Engine.getErrorString(Result));
    }

    UE_LOG_ERROR("[AudioSystem] Failed to initialize SoLoud MiniAudio backend after all attempts.");
    UE_LOG_WARNING("[AudioSystem] Audio will be disabled. Check the default playback device, Windows audio service, or exclusive device access.");
    return false;
}

void FAudioSystem::Shutdown()
{
    if (!Impl || !Impl->bInitialized)
    {
        return;
    }

    Impl->Engine.stopAll();
    Impl->StreamCache.clear();
    Impl->SFXCache.clear();
    Impl->ActiveSFXHandles.clear();
    Impl->CooldownRemaining.clear();
    Impl->BGMHandle = 0;
    Impl->Engine.deinit();
    Impl->bInitialized = false;
}

void FAudioSystem::Tick(float DeltaTime)
{
    if (!Impl->bInitialized)
    {
        return;
    }

    for (auto& Pair : Impl->CooldownRemaining)
    {
        Pair.second = std::max(0.0f, Pair.second - DeltaTime);
    }

    for (auto& Pair : Impl->ActiveSFXHandles)
    {
        TArray<SoLoud::handle>& Handles = Pair.second;
        Handles.erase(
            std::remove_if(
                Handles.begin(),
                Handles.end(),
                [this](SoLoud::handle Handle)
                {
                    return !Impl->Engine.isValidVoiceHandle(Handle);
                }),
            Handles.end());
    }

    if (Impl->bListenerDirty)
    {
        Impl->Engine.update3dAudio();
        Impl->bListenerDirty = false;
    }
}

bool FAudioSystem::IsInitialized() const
{
    return Impl->bInitialized;
}

void FAudioSystem::ReloadSoundRegistry()
{
    Impl->SoundRegistry.clear();

    const std::filesystem::path RegistryPaths[] = {
        std::filesystem::path(FPaths::SettingsDir()) / L"Sound.ini",
        std::filesystem::path(FPaths::RootDir()) / L"Asset" / L"Audio" / L"SoundRegistry.ini"
    };

    for (const std::filesystem::path& RegistryPath : RegistryPaths)
    {
        std::ifstream File(RegistryPath);
        if (!File.is_open())
        {
            continue;
        }

        FString Line;
        while (std::getline(File, Line))
        {
            Line = TrimAudioLine(Line);
            if (Line.empty() || Line[0] == '#' || Line[0] == ';' || Line[0] == '[')
            {
                continue;
            }

            const size_t Equals = Line.find('=');
            if (Equals == FString::npos)
            {
                continue;
            }

            const FString Key = TrimAudioLine(Line.substr(0, Equals));
            const FString Path = TrimAudioLine(Line.substr(Equals + 1));
            if (!Key.empty() && !Path.empty())
            {
                Impl->SoundRegistry[Key] = Path;
            }
        }
    }

    UE_LOG("[AudioSystem] Sound registry loaded. Count=%d", static_cast<int32>(Impl->SoundRegistry.size()));
}

void FAudioSystem::RegisterSound(const FString& Key, const FString& Path)
{
    if (Key.empty() || Path.empty())
    {
        return;
    }

    Impl->SoundRegistry[Key] = Path;
}

FString FAudioSystem::ResolveSoundPath(const FString& KeyOrPath) const
{
    return Impl->ResolveSoundPath(KeyOrPath);
}

void FAudioSystem::PlayBGM(const FString& KeyOrPath, float FadeInSeconds)
{
    if (!Impl->bInitialized)
    {
        UE_LOG_WARNING("[AudioSystem] PlayBGM requested while audio is not initialized. Retrying initialization.");
        if (!Initialize())
        {
            return;
        }
    }

    const FString NormalizedPath = Impl->ResolveSoundPath(KeyOrPath);
    SoLoud::Wav* Clip = Impl->LoadSFX(NormalizedPath, true);
    if (!Clip)
    {
        UE_LOG_WARNING("[AudioSystem] PlayBGM failed to load clip: %s", NormalizedPath.c_str());
        return;
    }

    StopBGM(FadeInSeconds > 0.0f ? std::min(FadeInSeconds, 0.25f) : 0.0f);

    const float StartVolume = FadeInSeconds > 0.0f ? 0.0f : Impl->BGMVolume;
    Impl->BGMHandle = Impl->Engine.playBackground(*Clip, StartVolume);
    Impl->CurrentBGMPath = NormalizedPath;
    UE_LOG("[AudioSystem] PlayBGM Path=%s Handle=%u Volume=%.2f FadeIn=%.2f Mode=DecodedClip", Impl->CurrentBGMPath.c_str(), Impl->BGMHandle, Impl->BGMVolume, FadeInSeconds);
    if (Impl->BGMHandle == 0)
    {
        UE_LOG_WARNING("[AudioSystem] PlayBGM failed to create a voice handle: %s", Impl->CurrentBGMPath.c_str());
        return;
    }
    if (FadeInSeconds > 0.0f && Impl->BGMHandle != 0)
    {
        Impl->Engine.fadeVolume(Impl->BGMHandle, Impl->BGMVolume, FadeInSeconds);
    }
}

void FAudioSystem::StopBGM(float FadeOutSeconds)
{
    if (!Impl->bInitialized || Impl->BGMHandle == 0)
    {
        return;
    }

    if (Impl->Engine.isValidVoiceHandle(Impl->BGMHandle))
    {
        if (FadeOutSeconds > 0.0f)
        {
            Impl->Engine.fadeVolume(Impl->BGMHandle, 0.0f, FadeOutSeconds);
            Impl->Engine.scheduleStop(Impl->BGMHandle, FadeOutSeconds);
        }
        else
        {
            Impl->Engine.stop(Impl->BGMHandle);
        }
    }

    Impl->BGMHandle = 0;
    Impl->CurrentBGMPath.clear();
}

FAudioHandle FAudioSystem::PlaySoundCue(
    const FString& KeyOrPath,
    bool bLoop,
    bool bSpatialized,
    const FVector& Position,
    float VolumeScale,
    float FadeInSeconds)
{
    if (!Impl->bInitialized)
    {
        return 0;
    }

    const FString NormalizedPath = Impl->ResolveSoundPath(KeyOrPath);
    if (!Impl->CanPlaySFX(NormalizedPath))
    {
        return 0;
    }

    SoLoud::Wav* Clip = Impl->LoadSFX(NormalizedPath, bLoop);
    if (!Clip)
    {
        return 0;
    }

    const float TargetVolume = ClampVolume(Impl->SFXVolume * VolumeScale);
    const float StartVolume = FadeInSeconds > 0.0f ? 0.0f : TargetVolume;
    const SoLoud::handle Handle = bSpatialized
        ? Impl->Engine.play3d(*Clip, Position.X, Position.Y, Position.Z, 0.0f, 0.0f, 0.0f, StartVolume)
        : Impl->Engine.play(*Clip, StartVolume);
    if (FadeInSeconds > 0.0f && Handle != 0)
    {
        Impl->Engine.fadeVolume(Handle, TargetVolume, FadeInSeconds);
    }
    Impl->MarkSFXPlayed(NormalizedPath, Handle);
    if (bSpatialized)
    {
        Impl->bListenerDirty = true;
    }
    return Handle;
}

FAudioHandle FAudioSystem::PlaySFX(const FString& KeyOrPath, float VolumeScale)
{
    return PlaySoundCue(KeyOrPath, false, false, FVector::ZeroVector, VolumeScale);
}

FAudioHandle FAudioSystem::PlaySFX3D(const FString& KeyOrPath, const FVector& Position, float VolumeScale)
{
    return PlaySoundCue(KeyOrPath, false, true, Position, VolumeScale);
}

void FAudioSystem::StopSound(FAudioHandle Handle, float FadeOutSeconds)
{
    if (!Impl->bInitialized || Handle == 0 || !Impl->Engine.isValidVoiceHandle(Handle))
    {
        return;
    }

    if (FadeOutSeconds > 0.0f)
    {
        Impl->Engine.fadeVolume(Handle, 0.0f, FadeOutSeconds);
        Impl->Engine.scheduleStop(Handle, FadeOutSeconds);
    }
    else
    {
        Impl->Engine.stop(Handle);
    }
}

bool FAudioSystem::IsHandleValid(FAudioHandle Handle) const
{
    return Impl->bInitialized && Handle != 0 && Impl->Engine.isValidVoiceHandle(Handle);
}

void FAudioSystem::SetSoundPosition(FAudioHandle Handle, const FVector& Position)
{
    if (!IsHandleValid(Handle))
    {
        return;
    }

    Impl->Engine.set3dSourcePosition(Handle, Position.X, Position.Y, Position.Z);
    Impl->bListenerDirty = true;
}

void FAudioSystem::SetMasterVolume(float Volume)
{
    Impl->MasterVolume = ClampVolume(Volume);
    if (Impl->bInitialized)
    {
        Impl->Engine.setGlobalVolume(Impl->MasterVolume);
    }
}

void FAudioSystem::SetBGMVolume(float Volume)
{
    Impl->BGMVolume = ClampVolume(Volume);
    if (Impl->bInitialized && Impl->BGMHandle != 0 && Impl->Engine.isValidVoiceHandle(Impl->BGMHandle))
    {
        Impl->Engine.setVolume(Impl->BGMHandle, Impl->BGMVolume);
    }
}

void FAudioSystem::SetSFXVolume(float Volume)
{
    Impl->SFXVolume = ClampVolume(Volume);
}

float FAudioSystem::GetMasterVolume() const
{
    return Impl->MasterVolume;
}

float FAudioSystem::GetBGMVolume() const
{
    return Impl->BGMVolume;
}

float FAudioSystem::GetSFXVolume() const
{
    return Impl->SFXVolume;
}

void FAudioSystem::SetListener(const FVector& Position, const FVector& Forward, const FVector& Up)
{
    if (!Impl->bInitialized)
    {
        return;
    }

    const FVector SafeForward = Forward.GetSafeNormal();
    const FVector SafeUp = Up.GetSafeNormal();
    Impl->Engine.set3dListenerParameters(
        Position.X, Position.Y, Position.Z,
        SafeForward.X, SafeForward.Y, SafeForward.Z,
        SafeUp.X, SafeUp.Y, SafeUp.Z);
    Impl->bListenerDirty = true;
}

void FAudioSystem::SetPlaybackPolicy(const FString& Path, const FAudioPlaybackPolicy& Policy)
{
    Impl->PlaybackPolicies[Impl->ResolveSoundPath(Path)] = Policy;
}

void FAudioSystem::ClearPlaybackPolicy(const FString& Path)
{
    Impl->PlaybackPolicies.erase(Impl->ResolveSoundPath(Path));
}

void FAudioSystem::StopAll()
{
    if (!Impl->bInitialized)
    {
        return;
    }

    Impl->Engine.stopAll();
    Impl->BGMHandle = 0;
    Impl->CurrentBGMPath.clear();
    Impl->ActiveSFXHandles.clear();
}
