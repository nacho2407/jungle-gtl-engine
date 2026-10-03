#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/Map.h"
#include "Core/Containers/String.h"
#include "Core/CoreTypes.h"
#include "Math/Vector.h"

#include <memory>

using FAudioHandle = uint32;

struct FAudioPlaybackPolicy
{
    // 같은 사운드가 한 프레임에 과하게 겹치는 것을 막기 위한 기본 정책입니다.
    int MaxConcurrent = 0;
    float CooldownSeconds = 0.0f;
    bool bStopOldestWhenFull = true;
};

struct FAudio3DSettings
{
    float MinDistance = 2.5f;
    float MaxDistance = 22.0f;
    int AttenuationModel = 2;
    float RolloffFactor = 1.0f;
};

class FAudioSystem
{
public:
    FAudioSystem();
    ~FAudioSystem();

    bool Initialize();
    void Shutdown();
    void Tick(float DeltaTime);

    bool IsInitialized() const;

    void ReloadSoundRegistry();
    void RegisterSound(const FString& Key, const FString& Path);
    FString ResolveSoundPath(const FString& KeyOrPath) const;

    void PlayBGM(const FString& KeyOrPath, float FadeInSeconds = 0.0f);
    void StopBGM(float FadeOutSeconds = 0.0f);

    FAudioHandle PlaySoundCue(
        const FString& KeyOrPath,
        bool bLoop = false,
        bool bSpatialized = false,
        const FVector& Position = FVector::ZeroVector,
        float VolumeScale = 1.0f,
        float FadeInSeconds = 0.0f,
        const FAudio3DSettings& SpatialSettings = FAudio3DSettings());
    FAudioHandle PlaySFX(const FString& KeyOrPath, float VolumeScale = 1.0f);
    FAudioHandle PlaySFX3D(const FString& KeyOrPath, const FVector& Position, float VolumeScale = 1.0f);
    void StopSound(FAudioHandle Handle, float FadeOutSeconds = 0.0f);
    bool IsHandleValid(FAudioHandle Handle) const;
    void SetSoundPosition(FAudioHandle Handle, const FVector& Position);

    void SetMasterVolume(float Volume);
    void SetBGMVolume(float Volume);
    void SetSFXVolume(float Volume);

    float GetMasterVolume() const;
    float GetBGMVolume() const;
    float GetSFXVolume() const;

    void SetListener(const FVector& Position, const FVector& Forward, const FVector& Up = FVector::UpVector);
    void SetPlaybackPolicy(const FString& Path, const FAudioPlaybackPolicy& Policy);
    void ClearPlaybackPolicy(const FString& Path);
    void StopAll();

private:
    struct FImpl;
    std::unique_ptr<FImpl> Impl;
};
