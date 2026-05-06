#include "Animation/ActorSequence.h"

#include "Component/ActorComponent.h"
#include "Component/ActorSequenceComponent.h"
#include "Core/ResourceManager.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    bool IsValueChannel(const FString& ChannelName)
    {
        return ChannelName.empty() || ChannelName == "Value";
    }

    bool GetScalarChannelValue(const FPropertyDescriptor& Property, const FString& ChannelName, float& OutValue)
    {
        if (!Property.ValuePtr)
        {
            return false;
        }

        if (Property.Type == EPropertyType::Float && IsValueChannel(ChannelName))
        {
            OutValue = *static_cast<float*>(Property.ValuePtr);
            return true;
        }

        if (Property.Type == EPropertyType::Vec3)
        {
            const FVector* Value = static_cast<const FVector*>(Property.ValuePtr);
            if (ChannelName == "X")
            {
                OutValue = Value->X;
                return true;
            }
            if (ChannelName == "Y")
            {
                OutValue = Value->Y;
                return true;
            }
            if (ChannelName == "Z")
            {
                OutValue = Value->Z;
                return true;
            }
        }

        if (Property.Type == EPropertyType::Color)
        {
            const FColor* Value = static_cast<const FColor*>(Property.ValuePtr);
            if (ChannelName == "R")
            {
                OutValue = Value->R;
                return true;
            }
            if (ChannelName == "G")
            {
                OutValue = Value->G;
                return true;
            }
            if (ChannelName == "B")
            {
                OutValue = Value->B;
                return true;
            }
            if (ChannelName == "A")
            {
                OutValue = Value->A;
                return true;
            }
        }

        return false;
    }

    bool SetScalarChannelValue(FPropertyDescriptor& Property, const FString& ChannelName, float NewValue)
    {
        if (!Property.ValuePtr)
        {
            return false;
        }

        if (Property.Type == EPropertyType::Float && IsValueChannel(ChannelName))
        {
            *static_cast<float*>(Property.ValuePtr) = NewValue;
            return true;
        }

        if (Property.Type == EPropertyType::Vec3)
        {
            FVector* Value = static_cast<FVector*>(Property.ValuePtr);
            if (ChannelName == "X")
            {
                Value->X = NewValue;
                return true;
            }
            if (ChannelName == "Y")
            {
                Value->Y = NewValue;
                return true;
            }
            if (ChannelName == "Z")
            {
                Value->Z = NewValue;
                return true;
            }
        }

        if (Property.Type == EPropertyType::Color)
        {
            FColor* Value = static_cast<FColor*>(Property.ValuePtr);
            if (ChannelName == "R")
            {
                Value->R = NewValue;
                return true;
            }
            if (ChannelName == "G")
            {
                Value->G = NewValue;
                return true;
            }
            if (ChannelName == "B")
            {
                Value->B = NewValue;
                return true;
            }
            if (ChannelName == "A")
            {
                Value->A = NewValue;
                return true;
            }
        }

        return false;
    }

    EActorSequenceTrackType GuessTrackType(EPropertyType PropertyType)
    {
        switch (PropertyType)
        {
        case EPropertyType::Vec3:
            return EActorSequenceTrackType::Vec3;
        case EPropertyType::Color:
            return EActorSequenceTrackType::Color;
        case EPropertyType::Float:
        default:
            return EActorSequenceTrackType::Float;
        }
    }
}

DEFINE_CLASS(UActorSequence, UObject)
REGISTER_FACTORY(UActorSequence)

DEFINE_CLASS(UActorSequencePlayer, UObject)
REGISTER_FACTORY(UActorSequencePlayer)

void UActorSequence::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);
    Ar << "Duration" << Duration;
    Ar << "Loop" << bLoop;

    TArray<FString> BindingGuids;
    TArray<FString> TargetGuids;
    TArray<FString> TargetNames;
    TArray<FString> TrackGuids;
    TArray<FString> TargetPropertyPaths;
    TArray<int32> TrackTypes;
    TArray<FString> SectionGuids;
    TArray<FString> ChannelNames;
    TArray<FString> CurveAssetPaths;
    TArray<float> StartTimes;
    TArray<float> Durations;
    TArray<float> PlayRates;
    TArray<int32> TrackLoops;
    TArray<int32> ApplyModes;
    TArray<int32> TimeMappingModes;

    if (Ar.IsSaving())
    {
        for (const FActorSequenceBinding& Binding : Bindings)
        {
            for (const FActorSequenceTrack& Track : Binding.Tracks)
            {
                for (const FActorSequenceSection& Section : Track.Sections)
                {
                    for (const FActorSequenceChannel& Channel : Section.Channels)
                    {
                        BindingGuids.push_back(Binding.Binding.BindingGuid.ToString());
                        TargetGuids.push_back(Binding.Binding.TargetObjectGuid.ToString());
                        TargetNames.push_back(Binding.Binding.TargetObjectName);
                        TrackGuids.push_back(Track.TrackGuid.ToString());
                        TargetPropertyPaths.push_back(Track.PropertyPath);
                        TrackTypes.push_back(static_cast<int32>(Track.TrackType));
                        SectionGuids.push_back(Section.SectionGuid.ToString());
                        ChannelNames.push_back(Channel.ChannelName);
                        CurveAssetPaths.push_back(Channel.Playback.CurveAssetPath);
                        StartTimes.push_back(Section.StartTime);
                        Durations.push_back(Section.Duration);
                        PlayRates.push_back(Section.PlayRate);
                        TrackLoops.push_back(Section.bLoop ? 1 : 0);
                        ApplyModes.push_back(static_cast<int32>(Channel.Playback.ApplyMode));
                        TimeMappingModes.push_back(static_cast<int32>(Channel.Playback.TimeMappingMode));
                    }
                }
            }
        }
    }

    Ar << "BindingGuids" << BindingGuids;
    Ar << "TargetGuids" << TargetGuids;
    Ar << "TargetNames" << TargetNames;
    Ar << "TrackGuids" << TrackGuids;
    Ar << "TargetPropertyPaths" << TargetPropertyPaths;
    Ar << "TrackTypes" << TrackTypes;
    Ar << "SectionGuids" << SectionGuids;
    Ar << "ChannelNames" << ChannelNames;
    Ar << "CurveAssetPaths" << CurveAssetPaths;
    Ar << "StartTimes" << StartTimes;
    Ar << "Durations" << Durations;
    Ar << "PlayRates" << PlayRates;
    Ar << "TrackLoops" << TrackLoops;
    Ar << "ApplyModes" << ApplyModes;
    Ar << "TimeMappingModes" << TimeMappingModes;

    if (Ar.IsLoading())
    {
        Bindings.clear();
        const size_t TrackCount = TargetPropertyPaths.size();

        for (size_t Index = 0; Index < TrackCount; ++Index)
        {
            FGuid BindingGuid = Index < BindingGuids.size()
                ? FGuid::FromString(BindingGuids[Index])
                : FGuid::NewGuid();
            if (!BindingGuid.IsValid())
            {
                BindingGuid = FGuid::NewGuid();
            }

            FGuid TargetGuid = Index < TargetGuids.size()
                ? FGuid::FromString(TargetGuids[Index])
                : FGuid();
            const FString TargetName = Index < TargetNames.size() ? TargetNames[Index] : "";
            const FString PropertyPath = TargetPropertyPaths[Index];
            const FGuid TrackGuid = Index < TrackGuids.size()
                ? FGuid::FromString(TrackGuids[Index])
                : FGuid::NewGuid();
            const FGuid SectionGuid = Index < SectionGuids.size()
                ? FGuid::FromString(SectionGuids[Index])
                : FGuid::NewGuid();

            FActorSequenceBinding* Binding = nullptr;
            for (FActorSequenceBinding& Existing : Bindings)
            {
                if (Existing.Binding.BindingGuid == BindingGuid)
                {
                    Binding = &Existing;
                    break;
                }
            }
            if (!Binding)
            {
                FActorSequenceBinding NewBinding;
                NewBinding.Binding.BindingGuid = BindingGuid;
                NewBinding.Binding.TargetObjectGuid = TargetGuid;
                NewBinding.Binding.TargetObjectName = TargetName;
                Bindings.push_back(NewBinding);
                Binding = &Bindings.back();
            }

            FActorSequenceTrack* Track = nullptr;
            for (FActorSequenceTrack& Existing : Binding->Tracks)
            {
                if (Existing.TrackGuid == TrackGuid)
                {
                    Track = &Existing;
                    break;
                }
            }
            if (!Track)
            {
                FActorSequenceTrack NewTrack;
                NewTrack.TrackGuid = TrackGuid.IsValid() ? TrackGuid : FGuid::NewGuid();
                NewTrack.PropertyPath = PropertyPath;
                NewTrack.TrackType = Index < TrackTypes.size()
                    ? static_cast<EActorSequenceTrackType>(TrackTypes[Index])
                    : EActorSequenceTrackType::Float;
                Binding->Tracks.push_back(NewTrack);
                Track = &Binding->Tracks.back();
            }

            FActorSequenceSection* Section = nullptr;
            for (FActorSequenceSection& Existing : Track->Sections)
            {
                if (Existing.SectionGuid == SectionGuid)
                {
                    Section = &Existing;
                    break;
                }
            }
            if (!Section)
            {
                FActorSequenceSection NewSection;
                NewSection.SectionGuid = SectionGuid.IsValid() ? SectionGuid : FGuid::NewGuid();
                NewSection.StartTime = Index < StartTimes.size() ? StartTimes[Index] : 0.0f;
                NewSection.Duration = Index < Durations.size() ? Durations[Index] : 1.0f;
                NewSection.PlayRate = Index < PlayRates.size() ? PlayRates[Index] : 1.0f;
                NewSection.bLoop = Index < TrackLoops.size() ? TrackLoops[Index] != 0 : false;
                Track->Sections.push_back(NewSection);
                Section = &Track->Sections.back();
            }

            FActorSequenceChannel Channel;
            Channel.ChannelName = Index < ChannelNames.size() ? ChannelNames[Index] : "Value";
            Channel.Playback.CurveAssetPath = Index < CurveAssetPaths.size() ? CurveAssetPaths[Index] : "";
            Channel.Playback.ApplyMode = Index < ApplyModes.size()
                ? static_cast<ECurveApplyMode>(ApplyModes[Index])
                : ECurveApplyMode::Direct;
            Channel.Playback.TimeMappingMode = Index < TimeMappingModes.size()
                ? static_cast<ECurveTimeMappingMode>(TimeMappingModes[Index])
                : ECurveTimeMappingMode::NormalizedTime;
            Section->Channels.push_back(Channel);
        }
    }
}

void UActorSequencePlayer::Initialize(
    UActorSequenceComponent* InOwner,
    UActorSequence* InSequence,
    ESequencePlayerContext InContext)
{
    OwnerComponent = InOwner;
    Sequence = InSequence;
    Context = InContext;
    CurrentTime = 0.0f;
    bPlaying = false;
    bPaused = false;
    MarkResolveDirty();
}

void UActorSequencePlayer::Play()
{
    bPlaying = true;
    bPaused = false;
}

void UActorSequencePlayer::Pause()
{
    if (bPlaying)
    {
        bPaused = true;
    }
}

void UActorSequencePlayer::Stop()
{
    bPlaying = false;
    bPaused = false;
    CurrentTime = 0.0f;
    ClearAppliedValues();
}

void UActorSequencePlayer::Tick(float DeltaTime)
{
    if (!Sequence || !IsOwnerLive() || !bPlaying || bPaused)
    {
        return;
    }

    if (bResolveDirty)
    {
        ResolveTracks();
    }

    CurrentTime += DeltaTime * PlayRate;
    if (Sequence->Duration > 0.0f && CurrentTime > Sequence->Duration)
    {
        if (Sequence->bLoop)
        {
            CurrentTime = std::fmod(CurrentTime, Sequence->Duration);
        }
        else
        {
            Stop();
            return;
        }
    }

    Evaluate(CurrentTime);
}

void UActorSequencePlayer::SetCurrentTime(float InCurrentTime)
{
    if (!Sequence || !IsOwnerLive())
    {
        return;
    }

    CurrentTime = std::max(0.0f, InCurrentTime);
    if (bResolveDirty)
    {
        ResolveTracks();
    }
    Evaluate(CurrentTime);
}

void UActorSequencePlayer::MarkResolveDirty()
{
    bResolveDirty = true;
}

void UActorSequencePlayer::ResolveTracks()
{
    ResolvedTracks.clear();
    bResolveDirty = false;

    if (!Sequence || !IsOwnerLive())
    {
        return;
    }

    for (FActorSequenceBinding& Binding : Sequence->Bindings)
    {
        for (FActorSequenceTrack& Track : Binding.Tracks)
        {
            for (FActorSequenceSection& Section : Track.Sections)
            {
                for (FActorSequenceChannel& Channel : Section.Channels)
                {
                    FResolvedActorSequenceTrack Resolved;
                    Resolved.SourceBinding = &Binding;
                    Resolved.SourceTrack = &Track;
                    Resolved.SourceSection = &Section;
                    Resolved.SourceChannel = &Channel;
                    Resolved.ResolvedComponent = ResolveComponent(Binding.Binding);
                    Resolved.ResolvedCurve = Channel.Playback.Curve
                        ? Channel.Playback.Curve
                        : FResourceManager::Get().LoadCurve(Channel.Playback.CurveAssetPath);
                    const bool bPropertyResolved = ResolveProperty(
                        Resolved.ResolvedComponent,
                        Track,
                        Channel,
                        Resolved.ResolvedProperty);
                    Resolved.bValid = Resolved.ResolvedComponent != nullptr
                        && Resolved.ResolvedCurve != nullptr
                        && bPropertyResolved
                        && CacheBaseValue(Resolved);
                    ResolvedTracks.push_back(Resolved);
                }
            }
        }
    }
}

void UActorSequencePlayer::Evaluate(float SequenceTime)
{
    for (FResolvedActorSequenceTrack& Resolved : ResolvedTracks)
    {
        if (!Resolved.bValid || !Resolved.SourceTrack || !Resolved.SourceSection || !Resolved.SourceChannel)
        {
            continue;
        }
        if (!IsResolvedTrackLive(Resolved))
        {
            Resolved.bValid = false;
            bResolveDirty = true;
            continue;
        }

        FCurvePlaybackDesc Playback = Resolved.SourceChannel->Playback;
        Playback.Curve = Resolved.ResolvedCurve;
        Playback.StartTime = Resolved.SourceSection->StartTime;
        Playback.Duration = Resolved.SourceSection->Duration;
        Playback.PlayRate = Resolved.SourceSection->PlayRate;
        Playback.bLoop = Resolved.SourceSection->bLoop;

        const FCurvePlaybackEvalResult Eval = FCurvePlaybackEvaluator::Evaluate(Playback, SequenceTime);
        if (Eval.bActive)
        {
            Resolved.LastValue = Eval.Value;
            ApplyFloat(Resolved, Eval.Value);
        }
    }
}

void UActorSequencePlayer::ClearAppliedValues()
{
    for (FResolvedActorSequenceTrack& Resolved : ResolvedTracks)
    {
        if (!IsResolvedTrackLive(Resolved))
        {
            Resolved.bValid = false;
            Resolved.LastValue = 0.0f;
            bResolveDirty = true;
            continue;
        }

        if (Resolved.bValid
            && Resolved.bHasBaseValue
            && Resolved.ResolvedProperty.ValuePtr)
        {
            const bool bShouldRestore = Context == ESequencePlayerContext::EditorPreview
                || (OwnerComponent && OwnerComponent->ShouldRestoreOnStop());
            if (bShouldRestore)
            {
                SetScalarChannelValue(
                    Resolved.ResolvedProperty,
                    Resolved.SourceChannel ? Resolved.SourceChannel->ChannelName : "Value",
                    Resolved.BaseFloatValue);
                if (Resolved.ResolvedComponent)
                {
                    Resolved.ResolvedComponent->PostEditChangeProperty(
                        { Resolved.ResolvedProperty.Name, EPropertyChangeType::ValueSet });
                }
            }
        }
        Resolved.LastValue = 0.0f;
    }
}

bool UActorSequencePlayer::IsOwnerLive() const
{
    if (!OwnerComponent || !UObjectManager::Get().ContainsObject(OwnerComponent))
    {
        return false;
    }

    AActor* OwnerActor = OwnerComponent->GetOwner();
    return OwnerActor
        && UObjectManager::Get().ContainsObject(OwnerActor)
        && !OwnerActor->IsPendingKill();
}

bool UActorSequencePlayer::IsResolvedTrackLive(const FResolvedActorSequenceTrack& Resolved) const
{
    if (!IsOwnerLive()
        || !Resolved.ResolvedComponent
        || !UObjectManager::Get().ContainsObject(Resolved.ResolvedComponent))
    {
        return false;
    }

    AActor* OwnerActor = OwnerComponent->GetOwner();
    const TArray<UActorComponent*>& Components = OwnerActor->GetComponents();
    return std::find(Components.begin(), Components.end(), Resolved.ResolvedComponent) != Components.end();
}

UActorComponent* UActorSequencePlayer::ResolveComponent(const FSequenceObjectBinding& Binding) const
{
    if (!IsOwnerLive())
    {
        return nullptr;
    }

    AActor* OwnerActor = OwnerComponent->GetOwner();
    for (UActorComponent* Component : OwnerActor->GetComponents())
    {
        if (Component
            && UObjectManager::Get().ContainsObject(Component)
            && Binding.TargetObjectGuid.IsValid()
            && Component->GetPersistentGuid() == Binding.TargetObjectGuid)
        {
            return Component;
        }
    }

    for (UActorComponent* Component : OwnerActor->GetComponents())
    {
        if (Component
            && UObjectManager::Get().ContainsObject(Component)
            && Component->GetName() == Binding.TargetObjectName)
        {
            return Component;
        }
    }

    return nullptr;
}

bool UActorSequencePlayer::ResolveProperty(
    UActorComponent* Component,
    const FActorSequenceTrack& Track,
    const FActorSequenceChannel& Channel,
    FPropertyDescriptor& OutProperty) const
{
    if (!Component || !UObjectManager::Get().ContainsObject(Component) || Track.PropertyPath.empty())
    {
        return false;
    }

    TArray<FPropertyDescriptor> Properties;
    Component->GetEditableProperties(Properties);

    for (const FPropertyDescriptor& Property : Properties)
    {
        if (!Property.Name || Track.PropertyPath != Property.Name)
        {
            continue;
        }
        if (!HasPropertyUsage(Property.UsageFlags, EPropertyUsageFlags::Animatable))
        {
            return false;
        }
        OutProperty = Property;
        float TestValue = 0.0f;
        return GetScalarChannelValue(OutProperty, Channel.ChannelName, TestValue);
    }

    return false;
}

bool UActorSequencePlayer::CacheBaseValue(FResolvedActorSequenceTrack& Resolved) const
{
    if (!IsResolvedTrackLive(Resolved)
        || !Resolved.ResolvedProperty.ValuePtr)
    {
        return false;
    }

    if (!GetScalarChannelValue(
        Resolved.ResolvedProperty,
        Resolved.SourceChannel ? Resolved.SourceChannel->ChannelName : "Value",
        Resolved.BaseFloatValue))
    {
        return false;
    }
    Resolved.bHasBaseValue = true;
    return true;
}

void UActorSequencePlayer::ApplyFloat(FResolvedActorSequenceTrack& Resolved, float CurveValue)
{
    if (!IsResolvedTrackLive(Resolved)
        || !Resolved.ResolvedProperty.ValuePtr)
    {
        Resolved.bValid = false;
        bResolveDirty = true;
        return;
    }

    float Result = CurveValue;
    switch (Resolved.SourceChannel->Playback.ApplyMode)
    {
    case ECurveApplyMode::Direct:
        Result = CurveValue;
        break;
    case ECurveApplyMode::Add:
        Result = Resolved.BaseFloatValue + CurveValue;
        break;
    case ECurveApplyMode::Multiply:
        Result = Resolved.BaseFloatValue * CurveValue;
        break;
    }

    if (!SetScalarChannelValue(
        Resolved.ResolvedProperty,
        Resolved.SourceChannel ? Resolved.SourceChannel->ChannelName : "Value",
        Result))
    {
        Resolved.bValid = false;
        bResolveDirty = true;
        return;
    }
    Resolved.ResolvedComponent->PostEditChangeProperty(
        {
            Resolved.ResolvedProperty.Name,
            Context == ESequencePlayerContext::EditorPreview
                ? EPropertyChangeType::Preview
                : EPropertyChangeType::Runtime
        });
}
