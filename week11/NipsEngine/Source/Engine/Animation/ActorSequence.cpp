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
    float ResolveSequenceStartTime(const UActorSequence* Sequence, float StartOffsetSeconds)
    {
        if (!Sequence)
        {
            return 0.0f;
        }

        const float StartTime = Sequence->StartTime;
        const float EndTime = Sequence->StartTime + std::max(0.0f, Sequence->Duration);
        return std::clamp(StartTime + std::max(0.0f, StartOffsetSeconds), StartTime, EndTime);
    }

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

        if (Property.Type == EPropertyType::Bool && IsValueChannel(ChannelName))
        {
            OutValue = *static_cast<bool*>(Property.ValuePtr) ? 1.0f : 0.0f;
            return true;
        }

        if (Property.Type == EPropertyType::Int && IsValueChannel(ChannelName))
        {
            OutValue = static_cast<float>(*static_cast<int32*>(Property.ValuePtr));
            return true;
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

        if (Property.Type == EPropertyType::Vec4)
        {
            const FVector4* Value = static_cast<const FVector4*>(Property.ValuePtr);
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
            if (ChannelName == "W")
            {
                OutValue = Value->W;
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

        if (Property.Type == EPropertyType::Bool && IsValueChannel(ChannelName))
        {
            *static_cast<bool*>(Property.ValuePtr) = NewValue >= 0.5f;
            return true;
        }

        if (Property.Type == EPropertyType::Int && IsValueChannel(ChannelName))
        {
            *static_cast<int32*>(Property.ValuePtr) = static_cast<int32>(std::round(NewValue));
            return true;
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

        if (Property.Type == EPropertyType::Vec4)
        {
            FVector4* Value = static_cast<FVector4*>(Property.ValuePtr);
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
            if (ChannelName == "W")
            {
                Value->W = NewValue;
                return true;
            }
        }

        return false;
    }

    EActorSequenceTrackType GuessTrackType(EPropertyType PropertyType)
    {
        switch (PropertyType)
        {
        case EPropertyType::Bool:
            return EActorSequenceTrackType::Bool;
        case EPropertyType::Int:
            return EActorSequenceTrackType::Int;
        case EPropertyType::Vec3:
            return EActorSequenceTrackType::Vec3;
        case EPropertyType::Vec4:
            return EActorSequenceTrackType::Vec4;
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
    Ar << "StartTime" << StartTime;
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
    TArray<int32> InlineCurveKeyCounts;
    TArray<float> InlineCurveTimes;
    TArray<float> InlineCurveValues;
    TArray<int32> InlineCurveInterpModes;
    TArray<int32> InlineCurveTangentModes;
    TArray<float> InlineCurveArriveTangents;
    TArray<float> InlineCurveLeaveTangents;

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

                        const UCurveFloatAsset* Curve = Channel.Playback.Curve
                            ? Channel.Playback.Curve
                            : (!Channel.Playback.CurveAssetPath.empty()
                                ? FResourceManager::Get().LoadCurve(Channel.Playback.CurveAssetPath)
                                : nullptr);
                        const FFloatCurve* FloatCurve = Curve ? &Curve->GetCurve() : nullptr;
                        InlineCurveKeyCounts.push_back(FloatCurve ? static_cast<int32>(FloatCurve->Keys.size()) : 0);
                        if (FloatCurve)
                        {
                            for (const FCurveKey& Key : FloatCurve->Keys)
                            {
                                InlineCurveTimes.push_back(Key.Time);
                                InlineCurveValues.push_back(Key.Value);
                                InlineCurveInterpModes.push_back(static_cast<int32>(Key.InterpMode));
                                InlineCurveTangentModes.push_back(static_cast<int32>(Key.TangentMode));
                                InlineCurveArriveTangents.push_back(Key.ArriveTangent);
                                InlineCurveLeaveTangents.push_back(Key.LeaveTangent);
                            }
                        }
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
    Ar << "InlineCurveKeyCounts" << InlineCurveKeyCounts;
    Ar << "InlineCurveTimes" << InlineCurveTimes;
    Ar << "InlineCurveValues" << InlineCurveValues;
    Ar << "InlineCurveInterpModes" << InlineCurveInterpModes;
    Ar << "InlineCurveTangentModes" << InlineCurveTangentModes;
    Ar << "InlineCurveArriveTangents" << InlineCurveArriveTangents;
    Ar << "InlineCurveLeaveTangents" << InlineCurveLeaveTangents;

    if (Ar.IsLoading())
    {
        Bindings.clear();
        const size_t TrackCount = TargetPropertyPaths.size();
        size_t InlineKeyOffset = 0;

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
                : ECurveApplyMode::Absolute;
            Channel.Playback.TimeMappingMode = Index < TimeMappingModes.size()
                ? static_cast<ECurveTimeMappingMode>(TimeMappingModes[Index])
                : ECurveTimeMappingMode::NormalizedTime;

            const int32 InlineKeyCount = Index < InlineCurveKeyCounts.size()
                ? std::max(0, InlineCurveKeyCounts[Index])
                : 0;
            if (InlineKeyCount > 0)
            {
                UCurveFloatAsset* InlineCurve = UObjectManager::Get().CreateObject<UCurveFloatAsset>();
                if (InlineCurve)
                {
                    FFloatCurve& FloatCurve = InlineCurve->GetMutableCurve();
                    FloatCurve.Keys.clear();
                    for (int32 KeyIndex = 0; KeyIndex < InlineKeyCount; ++KeyIndex)
                    {
                        const size_t SourceIndex = InlineKeyOffset + static_cast<size_t>(KeyIndex);
                        FCurveKey Key;
                        Key.Time = SourceIndex < InlineCurveTimes.size() ? InlineCurveTimes[SourceIndex] : 0.0f;
                        Key.Value = SourceIndex < InlineCurveValues.size() ? InlineCurveValues[SourceIndex] : 0.0f;
                        Key.InterpMode = SourceIndex < InlineCurveInterpModes.size()
                            ? static_cast<ECurveInterpMode>(InlineCurveInterpModes[SourceIndex])
                            : ECurveInterpMode::Cubic;
                        Key.TangentMode = SourceIndex < InlineCurveTangentModes.size()
                            ? static_cast<ECurveTangentMode>(InlineCurveTangentModes[SourceIndex])
                            : ECurveTangentMode::Auto;
                        Key.ArriveTangent = SourceIndex < InlineCurveArriveTangents.size() ? InlineCurveArriveTangents[SourceIndex] : 0.0f;
                        Key.LeaveTangent = SourceIndex < InlineCurveLeaveTangents.size() ? InlineCurveLeaveTangents[SourceIndex] : 0.0f;
                        FloatCurve.Keys.push_back(Key);
                    }
                    FloatCurve.SortKeys();
                    Channel.Playback.Curve = InlineCurve;
                }
            }
            InlineKeyOffset += static_cast<size_t>(InlineKeyCount);
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
    CurrentTime = ResolveSequenceStartTime(Sequence, StartOffsetSeconds);
    bPlaying = false;
    bPaused = false;
    MarkResolveDirty();
}

void UActorSequencePlayer::Play()
{
    if (Sequence)
    {
        const float StartTime = Sequence->StartTime;
        const float EndTime = Sequence->StartTime + std::max(0.0f, Sequence->Duration);
        if (CurrentTime < StartTime || CurrentTime >= EndTime)
        {
            CurrentTime = ResolveSequenceStartTime(Sequence, StartOffsetSeconds);
        }
    }
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
    CurrentTime = ResolveSequenceStartTime(Sequence, StartOffsetSeconds);
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

    const float StartTime = Sequence->StartTime;
    const float EndTime = Sequence->StartTime + std::max(0.0f, Sequence->Duration);
    if (CurrentTime < StartTime)
    {
        CurrentTime = StartTime;
    }

    CurrentTime += DeltaTime * PlayRate;
    if (Sequence->Duration > 0.0f && CurrentTime > EndTime)
    {
        if (Sequence->bLoop)
        {
            CurrentTime = StartTime + std::fmod(CurrentTime - StartTime, Sequence->Duration);
        }
        else
        {
            CurrentTime = EndTime;
            Evaluate(CurrentTime);
            if (bPauseAtEnd)
            {
                bPlaying = false;
                bPaused = true;
                return;
            }
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

    const float StartTime = Sequence->StartTime;
    const float EndTime = Sequence->StartTime + std::max(0.0f, Sequence->Duration);
    CurrentTime = std::clamp(InCurrentTime, StartTime, EndTime);
    if (bResolveDirty)
    {
        ResolveTracks();
    }
    Evaluate(CurrentTime);
}

void UActorSequencePlayer::SetStartOffset(float InStartOffsetSeconds)
{
    StartOffsetSeconds = std::max(0.0f, InStartOffsetSeconds);
    if (!bPlaying && Sequence)
    {
        CurrentTime = ResolveSequenceStartTime(Sequence, StartOffsetSeconds);
    }
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
                    Resolved.ResolvedObject = ResolveObject(Binding.Binding);
                    Resolved.ResolvedCurve = Channel.Playback.Curve
                        ? Channel.Playback.Curve
                        : FResourceManager::Get().LoadCurve(Channel.Playback.CurveAssetPath);
                    const bool bPropertyResolved = ResolveProperty(
                        Resolved.ResolvedObject,
                        Track,
                        Channel,
                        Resolved.ResolvedProperty);
                    Resolved.bValid = Resolved.ResolvedObject != nullptr
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
            SetScalarChannelValue(
                Resolved.ResolvedProperty,
                Resolved.SourceChannel ? Resolved.SourceChannel->ChannelName : "Value",
                Resolved.BaseFloatValue);
            if (Resolved.ResolvedObject)
            {
                Resolved.ResolvedObject->PostEditChangeProperty(
                    { Resolved.ResolvedProperty.Name, EPropertyChangeType::ValueSet });
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
        || !Resolved.ResolvedObject
        || !UObjectManager::Get().ContainsObject(Resolved.ResolvedObject))
    {
        return false;
    }

    AActor* OwnerActor = OwnerComponent->GetOwner();
    if (Resolved.ResolvedObject == OwnerActor)
    {
        return true;
    }

    const TArray<UActorComponent*>& Components = OwnerActor->GetComponents();
    UActorComponent* ResolvedComponent = Cast<UActorComponent>(Resolved.ResolvedObject);
    return ResolvedComponent
        && std::find(Components.begin(), Components.end(), ResolvedComponent) != Components.end();
}

UObject* UActorSequencePlayer::ResolveObject(const FSequenceObjectBinding& Binding) const
{
    if (!IsOwnerLive())
    {
        return nullptr;
    }

    AActor* OwnerActor = OwnerComponent->GetOwner();
    if (OwnerActor && OwnerActor->GetName() == Binding.TargetObjectName)
    {
        return OwnerActor;
    }

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
    UObject* Object,
    const FActorSequenceTrack& Track,
    const FActorSequenceChannel& Channel,
    FPropertyDescriptor& OutProperty) const
{
    if (!Object || !UObjectManager::Get().ContainsObject(Object) || Track.PropertyPath.empty())
    {
        return false;
    }

    TArray<FPropertyDescriptor> Properties;
    Object->GetEditableProperties(Properties);

    for (const FPropertyDescriptor& Property : Properties)
    {
        if (!Property.Name || Track.PropertyPath != Property.Name)
        {
            continue;
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
    case ECurveApplyMode::Absolute:
        Result = CurveValue;
        break;
    case ECurveApplyMode::Additive:
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
    Resolved.ResolvedObject->PostEditChangeProperty(
        {
            Resolved.ResolvedProperty.Name,
            Context == ESequencePlayerContext::EditorPreview
                ? EPropertyChangeType::Preview
                : EPropertyChangeType::Runtime
        });
}
