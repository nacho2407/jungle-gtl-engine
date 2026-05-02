#include "Component/SoundComponent.h"

#include "Engine/Runtime/Engine.h"
#include "Serialization/Archive.h"

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
}

void USoundComponent::Play()
{
    if (!GEngine || SoundKeyOrPath.empty())
    {
        return;
    }

    Stop();
    ActiveHandle = GEngine->GetAudioSystem().PlaySoundCue(
        SoundKeyOrPath,
        bLoop,
        bSpatialized,
        GetWorldLocation(),
        VolumeScale,
        FadeInSeconds);
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
