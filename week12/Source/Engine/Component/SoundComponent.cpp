#include "Component/SoundComponent.h"

#include "Engine/Runtime/Engine.h"
#include "Serialization/Archive.h"

#include <algorithm>


void USoundComponent::BeginPlay()
{
	USceneComponent::BeginPlay();

	if (bPlayOnBeginPlay)
	{
		Play();
	}
}

void USoundComponent::EndPlay()
{
	Stop();
	USceneComponent::EndPlay();
}



void USoundComponent::Play()
{
	if (!GEngine || SoundKeyOrPath.empty())
	{
		return;
	}

	Stop();
	FAudio3DSettings SpatialSettings;
	SpatialSettings.MinDistance = MinDistance;
	SpatialSettings.MaxDistance = MaxDistance;
	SpatialSettings.AttenuationModel = AttenuationModel;
	SpatialSettings.RolloffFactor = RolloffFactor;
	ActiveHandle = GEngine->GetAudioSystem().PlaySoundCue(
		SoundKeyOrPath,
		bLoop,
		bSpatialized,
		GetWorldLocation(),
		VolumeScale,
		FadeInSeconds,
		SpatialSettings);
}

void USoundComponent::Stop()
{
	if (!GEngine || ActiveHandle == 0)
	{
		ActiveHandle = 0;
		return;
	}

	GEngine->GetAudioSystem().StopSound(ActiveHandle, FadeOutSeconds);
	ActiveHandle = 0;
}

bool USoundComponent::IsPlaying() const
{
	return GEngine && ActiveHandle != 0 && GEngine->GetAudioSystem().IsHandleValid(ActiveHandle);
}

void USoundComponent::Set3DMinMaxDistance(float InMinDistance, float InMaxDistance)
{
	MinDistance = std::max(0.0f, InMinDistance);
	MaxDistance = std::max(MinDistance + 0.01f, InMaxDistance);
}

void USoundComponent::Set3DAttenuation(int InAttenuationModel, float InRolloffFactor)
{
	AttenuationModel = std::clamp(InAttenuationModel, 0, 3);
	RolloffFactor = std::max(0.0f, InRolloffFactor);
}

void USoundComponent::TickComponent(float DeltaTime)
{
	(void)DeltaTime;

	if (!GEngine || ActiveHandle == 0)
	{
		return;
	}

	if (!GEngine->GetAudioSystem().IsHandleValid(ActiveHandle))
	{
		ActiveHandle = 0;
		return;
	}

	if (bSpatialized)
	{
		GEngine->GetAudioSystem().SetSoundPosition(ActiveHandle, GetWorldLocation());
	}
}
