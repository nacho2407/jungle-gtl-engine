#include "Component/SoundComponent.h"

#include "Engine/Runtime/Engine.h"
#include "Serialization/Archive.h"

#include <algorithm>

DEFINE_CLASS(USoundComponent, USceneComponent)
REGISTER_FACTORY(USoundComponent)

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

void USoundComponent::Serialize(FArchive& Ar)
{
    USceneComponent::Serialize(Ar);
    Ar << "Sound" << SoundKeyOrPath;
    Ar << "Play On BeginPlay" << bPlayOnBeginPlay;
    Ar << "Loop" << bLoop;
    Ar << "Spatialized" << bSpatialized;
    Ar << "Volume Scale" << VolumeScale;
    Ar << "Fade In" << FadeInSeconds;
    Ar << "Fade Out" << FadeOutSeconds;
    Ar << "3D Min Distance" << MinDistance;
    Ar << "3D Max Distance" << MaxDistance;
    Ar << "3D Attenuation Model" << AttenuationModel;
    Ar << "3D Rolloff Factor" << RolloffFactor;
}

void USoundComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Sound", EPropertyType::String, &SoundKeyOrPath });
    OutProps.push_back({ "Play On BeginPlay", EPropertyType::Bool, &bPlayOnBeginPlay });
    OutProps.push_back({ "Loop", EPropertyType::Bool, &bLoop });
    OutProps.push_back({ "Spatialized", EPropertyType::Bool, &bSpatialized });
    OutProps.push_back({ "Volume Scale", EPropertyType::Float, &VolumeScale, 0.0f, 2.0f, 0.01f });
    OutProps.push_back({ "Fade In", EPropertyType::Float, &FadeInSeconds, 0.0f, 10.0f, 0.01f });
    OutProps.push_back({ "Fade Out", EPropertyType::Float, &FadeOutSeconds, 0.0f, 10.0f, 0.01f });
    OutProps.push_back({ "3D Min Distance", EPropertyType::Float, &MinDistance, 0.0f, 100.0f, 0.1f });
    OutProps.push_back({ "3D Max Distance", EPropertyType::Float, &MaxDistance, 0.1f, 500.0f, 0.1f });
    OutProps.push_back({ "3D Attenuation Model", EPropertyType::Int, &AttenuationModel, 0.0f, 3.0f, 1.0f });
    OutProps.push_back({ "3D Rolloff Factor", EPropertyType::Float, &RolloffFactor, 0.0f, 8.0f, 0.1f });
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
