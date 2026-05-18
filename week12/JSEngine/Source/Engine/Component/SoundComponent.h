#pragma once

#include "Audio/AudioSystem.h"
#include "Component/SceneComponent.h"

UCLASS(SpawnableComponent, DisplayName = "Sound Component", Category = "System")
class USoundComponent : public USceneComponent
{
public:
	GENERATED_BODY(USoundComponent, USceneComponent)

	void BeginPlay() override;
	void EndPlay() override;

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
	UPROPERTY(DisplayName = "Sound")
	FString SoundKeyOrPath;

	UPROPERTY(DisplayName = "Play On BeginPlay")
	bool bPlayOnBeginPlay = false;

	UPROPERTY(DisplayName = "Loop")
	bool bLoop = false;

	UPROPERTY(DisplayName = "Spatialized")
	bool bSpatialized = true;

	UPROPERTY(DisplayName = "Volume Scale", Min = 0.0f, Max = 2.0f, Speed = 0.01f)
	float VolumeScale = 1.0f;

	UPROPERTY(DisplayName = "Fade In", Min = 0.0f, Max = 10.0f, Speed = 0.01f)
	float FadeInSeconds = 0.0f;

	UPROPERTY(DisplayName = "Fade Out", Min = 0.0f, Max = 10.0f, Speed = 0.01f)
	float FadeOutSeconds = 0.0f;

	UPROPERTY(DisplayName = "3D Min Distance", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float MinDistance = 2.5f;

	UPROPERTY(DisplayName = "3D Max Distance", Min = 0.1f, Max = 500.0f, Speed = 0.1f)
	float MaxDistance = 22.0f;

	UPROPERTY(DisplayName = "3D Attenuation Model", Min = 0.0f, Max = 3.0f, Speed = 1.0f)
	int AttenuationModel = 2;

	UPROPERTY(DisplayName = "3D Rolloff Factor", Min = 0.0f, Max = 8.0f, Speed = 0.1f)
	float RolloffFactor = 1.0f;

	FAudioHandle ActiveHandle = 0;
};
