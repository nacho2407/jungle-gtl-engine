#pragma once

#include "SoundComponent.generated.h"

#include "Audio/AudioSystem.h"
#include "Component/SceneComponent.h"

UCLASS()
class USoundComponent : public USceneComponent
{
    GENERATED_BODY_USoundComponent()
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
    void SetPlayOnBeginPlay(bool bEnabled) { bPlayOnBeginPlay = bEnabled; }
    bool IsPlayOnBeginPlay() const { return bPlayOnBeginPlay; }
    void SetLoop(bool bEnabled) { bLoop = bEnabled; }
    bool IsLooping() const { return bLoop; }
    void SetSpatialized(bool bEnabled) { bSpatialized = bEnabled; }
    bool IsSpatialized() const { return bSpatialized; }
    void SetVolumeScale(float InVolumeScale) { VolumeScale = InVolumeScale; }
    float GetVolumeScale() const { return VolumeScale; }
    void Set3DMinMaxDistance(float InMinDistance, float InMaxDistance);
    float Get3DMinDistance() const { return MinDistance; }
    float Get3DMaxDistance() const { return MaxDistance; }
    void Set3DAttenuation(int InAttenuationModel, float InRolloffFactor);
    int Get3DAttenuationModel() const { return AttenuationModel; }
    float Get3DRolloffFactor() const { return RolloffFactor; }

protected:
    void TickComponent(float DeltaTime) override;

private:
    // SoundRegistry 키 또는 Asset/Audio 기준 파일 경로를 넣습니다.
    UPROPERTY(EditAnywhere, Category="Audio", DisplayName="Sound")
    FString SoundKeyOrPath;
    UPROPERTY(EditAnywhere, Category="Playback", DisplayName="Play On Begin Play")
    bool bPlayOnBeginPlay = false;
    UPROPERTY(EditAnywhere, Category="Playback", DisplayName="Loop")
    bool bLoop = false;
    UPROPERTY(EditAnywhere, Category="Attenuation", DisplayName="Spatialized")
    bool bSpatialized = true;
    UPROPERTY(EditAnywhere, Category="Audio", DisplayName="Volume Scale")
    float VolumeScale = 1.0f;
    UPROPERTY(EditAnywhere, Category="Playback", DisplayName="Fade In")
    float FadeInSeconds = 0.0f;
    UPROPERTY(EditAnywhere, Category="Playback", DisplayName="Fade Out")
    float FadeOutSeconds = 0.0f;
    UPROPERTY(EditAnywhere, Category="Attenuation", DisplayName="Min Distance")
    float MinDistance = 2.5f;
    UPROPERTY(EditAnywhere, Category="Attenuation", DisplayName="Max Distance")
    float MaxDistance = 22.0f;
    UPROPERTY(EditAnywhere, Category="Attenuation", DisplayName="Attenuation Model")
    int AttenuationModel = 2;
    UPROPERTY(EditAnywhere, Category="Attenuation", DisplayName="Rolloff Factor")
    float RolloffFactor = 1.0f;
    FAudioHandle ActiveHandle = 0;
};
