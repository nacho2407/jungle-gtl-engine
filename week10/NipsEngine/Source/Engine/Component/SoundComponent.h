#pragma once

#include "Audio/AudioSystem.h"
#include "Component/SceneComponent.h"

class USoundComponent : public USceneComponent
{
public:
    DECLARE_CLASS(USoundComponent, USceneComponent)

    void BeginPlay() override;
    void EndPlay() override;
    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

    void Play();
    void Stop();
    bool IsPlaying() const;

    void SetSound(const FString& InSoundKeyOrPath) { SoundKeyOrPath = InSoundKeyOrPath; }
    const FString& GetSound() const { return SoundKeyOrPath; }

protected:
    void TickComponent(float DeltaTime) override;

private:
    // SoundRegistry 키 또는 Asset/Audio 기준 파일 경로를 넣습니다.
    FString SoundKeyOrPath;
    bool bPlayOnBeginPlay = false;
    bool bLoop = false;
    bool bSpatialized = true;
    float VolumeScale = 1.0f;
    float FadeInSeconds = 0.0f;
    float FadeOutSeconds = 0.0f;
    FAudioHandle ActiveHandle = 0;
};
