#include "Component/ActorSequenceComponent.h"

#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <algorithm>

DEFINE_CLASS(UActorSequenceComponent, UActorComponent)
REGISTER_FACTORY(UActorSequenceComponent)

void UActorSequenceComponent::BeginPlay()
{
    UActorComponent::BeginPlay();
    EnsureSequence();
    EnsureSequencePlayer(ESequencePlayerContext::Runtime);

    if (bAutoPlay && SequencePlayer)
    {
        SequencePlayer->Play();
    }
}

void UActorSequenceComponent::EndPlay()
{
    Stop();
    StopPreview();
}

void UActorSequenceComponent::ExecutePreviewTick(float DeltaTime)
{
    EnsureSequence();
    EnsurePreviewSequencePlayer();
    if (PreviewSequencePlayer)
    {
        PreviewSequencePlayer->Tick(DeltaTime);
    }
}

void UActorSequenceComponent::Play()
{
    EnsureSequence();
    EnsureSequencePlayer(ESequencePlayerContext::Runtime);
    if (SequencePlayer)
    {
        SequencePlayer->Play();
    }
}

void UActorSequenceComponent::Pause()
{
    if (SequencePlayer)
    {
        SequencePlayer->Pause();
    }
}

void UActorSequenceComponent::Stop()
{
    if (SequencePlayer)
    {
        SequencePlayer->Stop();
    }
}

void UActorSequenceComponent::PlayPreview()
{
    EnsureSequence();
    EnsurePreviewSequencePlayer();
    if (PreviewSequencePlayer)
    {
        PreviewSequencePlayer->Play();
    }
}

void UActorSequenceComponent::PausePreview()
{
    if (PreviewSequencePlayer)
    {
        PreviewSequencePlayer->Pause();
    }
}

void UActorSequenceComponent::StopPreview()
{
    if (PreviewSequencePlayer)
    {
        PreviewSequencePlayer->Stop();
    }
}

void UActorSequenceComponent::SetPreviewTime(float InTime)
{
    EnsureSequence();
    EnsurePreviewSequencePlayer();
    if (PreviewSequencePlayer)
    {
        PreviewSequencePlayer->SetCurrentTime(InTime);
    }
}

bool UActorSequenceComponent::AddFloatTrack(const FActorSequenceFloatTrackDesc& Desc)
{
    EnsureSequence();

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor || Desc.TargetObjectName.empty() || Desc.TargetPropertyPath.empty())
    {
        return false;
    }

    UActorComponent* TargetComponent = nullptr;
    for (UActorComponent* Component : OwnerActor->GetComponents())
    {
        if (Component && Component->GetName() == Desc.TargetObjectName)
        {
            TargetComponent = Component;
            break;
        }
    }

    if (!TargetComponent)
    {
        return false;
    }

    TArray<FPropertyDescriptor> Properties;
    TargetComponent->GetEditableProperties(Properties);

    const bool bCanAnimateProperty = std::any_of(
        Properties.begin(),
        Properties.end(),
        [&Desc](const FPropertyDescriptor& Property)
        {
            return Property.Name
                && Desc.TargetPropertyPath == Property.Name
                && Property.Type == EPropertyType::Float
                && HasPropertyUsage(Property.UsageFlags, EPropertyUsageFlags::Animatable);
        });

    if (!bCanAnimateProperty)
    {
        return false;
    }

    TargetComponent->EnsurePersistentGuid();

    FActorSequenceBinding Binding;
    Binding.Binding.BindingGuid = FGuid::NewGuid();
    Binding.Binding.TargetObjectGuid = TargetComponent->GetPersistentGuid();
    Binding.Binding.TargetObjectName = TargetComponent->GetName();

    FActorSequenceTrack Track;
    Track.TrackGuid = FGuid::NewGuid();
    Track.PropertyPath = Desc.TargetPropertyPath;
    Track.TrackType = EActorSequenceTrackType::Float;

    FActorSequenceSection Section;
    Section.SectionGuid = FGuid::NewGuid();
    Section.StartTime = std::max(0.0f, Desc.StartTime);
    Section.Duration = std::max(0.0f, Desc.Duration);
    Section.PlayRate = Desc.PlayRate;
    Section.bLoop = Desc.bLoop;

    FActorSequenceChannel Channel;
    Channel.ChannelName = "Value";
    Channel.Playback.CurveAssetPath = Desc.CurveAssetPath;
    Channel.Playback.Curve = Desc.Curve;
    Channel.Playback.ApplyMode = Desc.ApplyMode;
    Channel.Playback.TimeMappingMode = Desc.TimeMappingMode;

    Section.Channels.push_back(Channel);
    Track.Sections.push_back(Section);
    Binding.Tracks.push_back(Track);
    Sequence->Bindings.push_back(Binding);
    MarkSequenceDirty();
    return true;
}

UActorSequence* UActorSequenceComponent::GetSequence()
{
    EnsureSequence();
    return Sequence;
}

UActorSequencePlayer* UActorSequenceComponent::GetSequencePlayer()
{
    EnsureSequence();
    EnsureSequencePlayer(ESequencePlayerContext::Runtime);
    return SequencePlayer;
}

UActorSequencePlayer* UActorSequenceComponent::GetPreviewSequencePlayer()
{
    EnsureSequence();
    EnsurePreviewSequencePlayer();
    return PreviewSequencePlayer;
}

void UActorSequenceComponent::MarkSequenceDirty()
{
    if (SequencePlayer)
    {
        SequencePlayer->MarkResolveDirty();
    }
    if (PreviewSequencePlayer)
    {
        PreviewSequencePlayer->MarkResolveDirty();
    }
}

void UActorSequenceComponent::TickComponent(float DeltaTime)
{
    if (SequencePlayer)
    {
        SequencePlayer->Tick(DeltaTime);
    }
}

void UActorSequenceComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);
    EnsureSequence();

    OutProps.push_back({ "Auto Play", EPropertyType::Bool, &bAutoPlay });
    OutProps.push_back({ "Restore On Stop", EPropertyType::Bool, &bRestoreOnStop });
    OutProps.push_back({ "Sequence Duration", EPropertyType::Float, &Sequence->Duration, 0.0f, 600.0f, 0.01f });
    OutProps.push_back({ "Sequence Loop", EPropertyType::Bool, &Sequence->bLoop });
}

void UActorSequenceComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);
    Ar << "AutoPlay" << bAutoPlay;
    Ar << "RestoreOnStop" << bRestoreOnStop;

    EnsureSequence();
    Ar.BeginObject("Sequence");
    Sequence->Serialize(Ar);
    Ar.EndObject();
}

void UActorSequenceComponent::PostDuplicate(UObject* Original)
{
    UActorComponent::PostDuplicate(Original);

    UActorSequenceComponent* SourceComponent = Cast<UActorSequenceComponent>(Original);
    if (!SourceComponent || !SourceComponent->Sequence)
    {
        return;
    }

    EnsureSequence();
    Sequence->Duration = SourceComponent->Sequence->Duration;
    Sequence->bLoop = SourceComponent->Sequence->bLoop;
    Sequence->Bindings = SourceComponent->Sequence->Bindings;

    for (FActorSequenceBinding& Binding : Sequence->Bindings)
    {
        Binding.Binding.BindingGuid = FGuid::NewGuid();
        for (FActorSequenceTrack& Track : Binding.Tracks)
        {
            Track.TrackGuid = FGuid::NewGuid();
            for (FActorSequenceSection& Section : Track.Sections)
            {
                Section.SectionGuid = FGuid::NewGuid();
            }
        }
    }
    MarkSequenceDirty();
}

void UActorSequenceComponent::EnsureSequence()
{
    if (!Sequence)
    {
        Sequence = UObjectManager::Get().CreateObject<UActorSequence>();
    }
}

void UActorSequenceComponent::EnsureSequencePlayer(ESequencePlayerContext Context)
{
    EnsureSequence();
    if (!SequencePlayer)
    {
        SequencePlayer = UObjectManager::Get().CreateObject<UActorSequencePlayer>();
        SequencePlayer->Initialize(this, Sequence, Context);
    }
}

void UActorSequenceComponent::EnsurePreviewSequencePlayer()
{
    EnsureSequence();
    if (!PreviewSequencePlayer)
    {
        PreviewSequencePlayer = UObjectManager::Get().CreateObject<UActorSequencePlayer>();
        PreviewSequencePlayer->Initialize(this, Sequence, ESequencePlayerContext::EditorPreview);
    }
}
