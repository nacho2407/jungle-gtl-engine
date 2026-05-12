#include "Editor/UI/EditorActorSequenceEditModel.h"

#include "Asset/CurveFloatAsset.h"
#include "Component/ActorComponent.h"
#include "Component/ActorSequenceComponent.h"
#include "Core/ResourceManager.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorActorSequenceTimeUtils.h"
#include "GameFramework/AActor.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    constexpr float MinSequenceDuration = 0.001f;

    bool IsValueChannel(const FString& ChannelName)
    {
        return ChannelName.empty() || ChannelName == "Value";
    }

    bool IsSequencerScalarProperty(const FPropertyDescriptor& Prop)
    {
        if (!Prop.Name || !Prop.ValuePtr)
        {
            return false;
        }

        return Prop.Type == EPropertyType::Bool
            || Prop.Type == EPropertyType::Int
            || Prop.Type == EPropertyType::Float
            || Prop.Type == EPropertyType::Vec3
            || Prop.Type == EPropertyType::Vec4
            || Prop.Type == EPropertyType::Color;
    }

    bool HasChannel(const FActorSequenceSection& Section, const char* ChannelName)
    {
        if (!ChannelName)
        {
            return true;
        }

        for (const FActorSequenceChannel& Channel : Section.Channels)
        {
            if (Channel.ChannelName == ChannelName)
            {
                return true;
            }
        }
        return false;
    }

    UCurveFloatAsset* CreateSequenceOwnedCurve(float InitialValue)
    {
        UCurveFloatAsset* Curve = UObjectManager::Get().CreateObject<UCurveFloatAsset>();
        if (!Curve)
        {
            return nullptr;
        }

        FFloatCurve& FloatCurve = Curve->GetMutableCurve();
        FloatCurve.Keys.clear();

        FCurveKey StartKey;
        StartKey.Time = 0.0f;
        StartKey.Value = InitialValue;
        StartKey.InterpMode = ECurveInterpMode::Cubic;
        StartKey.TangentMode = ECurveTangentMode::Auto;
        FloatCurve.Keys.push_back(StartKey);

        FCurveKey EndKey = StartKey;
        EndKey.Time = 1.0f;
        FloatCurve.Keys.push_back(EndKey);
        FloatCurve.SortKeys();
        return Curve;
    }

    FSequenceCurvePlaybackDesc MakeDefaultPlayback(float InitialValue)
    {
        FSequenceCurvePlaybackDesc Playback;
        Playback.Curve = CreateSequenceOwnedCurve(InitialValue);
        Playback.ApplyMode = ECurveApplyMode::Absolute;
        Playback.TimeMappingMode = ECurveTimeMappingMode::CurveTime;
        return Playback;
    }

    bool ReadScalarChannelValue(const FPropertyDescriptor& Property, const FString& ChannelName, float& OutValue)
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

    UObject* ResolveObject(UActorSequenceComponent* SequenceComp, const FActorSequenceBinding& Binding)
    {
        AActor* Owner = FEditorActorSequenceEditModel::GetLiveOwner(SequenceComp);
        if (!Owner)
        {
            return nullptr;
        }

        if (Owner->GetName() == Binding.Binding.TargetObjectName)
        {
            return Owner;
        }

        for (UActorComponent* Component : Owner->GetComponents())
        {
            if (Component
                && UObjectManager::Get().ContainsObject(Component)
                && Binding.Binding.TargetObjectGuid.IsValid()
                && Component->GetPersistentGuid() == Binding.Binding.TargetObjectGuid)
            {
                return Component;
            }
        }

        for (UActorComponent* Component : Owner->GetComponents())
        {
            if (Component
                && UObjectManager::Get().ContainsObject(Component)
                && Component->GetName() == Binding.Binding.TargetObjectName)
            {
                return Component;
            }
        }

        return nullptr;
    }

    bool ResolveProperty(
        UActorSequenceComponent* SequenceComp,
        const FActorSequenceBinding& Binding,
        const FActorSequenceTrack& Track,
        const FActorSequenceChannel& Channel,
        FPropertyDescriptor& OutProperty)
    {
        UObject* Object = ResolveObject(SequenceComp, Binding);
        if (!Object || Track.PropertyPath.empty())
        {
            return false;
        }

        TArray<FPropertyDescriptor> Properties;
        FEditorActorSequenceEditModel::CollectAnimatableScalarProperties(Object, Properties);
        for (const FPropertyDescriptor& Property : Properties)
        {
            if (!Property.Name || Track.PropertyPath != Property.Name)
            {
                continue;
            }

            float TestValue = 0.0f;
            if (!ReadScalarChannelValue(Property, Channel.ChannelName, TestValue))
            {
                return false;
            }

            OutProperty = Property;
            return true;
        }

        return false;
    }

}

bool FEditorActorSequenceEditModel::IsSequenceComponentLive(UActorSequenceComponent* SequenceComp)
{
    if (!SequenceComp || !UObjectManager::Get().ContainsObject(SequenceComp))
    {
        return false;
    }

    AActor* Owner = SequenceComp->GetOwner();
    return Owner
        && UObjectManager::Get().ContainsObject(Owner)
        && !Owner->IsPendingKill()
        && SequenceComp->GetSequence() != nullptr;
}

AActor* FEditorActorSequenceEditModel::GetLiveOwner(UActorSequenceComponent* SequenceComp)
{
    if (!SequenceComp || !UObjectManager::Get().ContainsObject(SequenceComp))
    {
        return nullptr;
    }

    AActor* Owner = SequenceComp->GetOwner();
    if (!Owner || !UObjectManager::Get().ContainsObject(Owner) || Owner->IsPendingKill())
    {
        return nullptr;
    }

    return Owner;
}

void FEditorActorSequenceEditModel::CollectAnimatableScalarProperties(
    UObject* Object,
    TArray<FPropertyDescriptor>& OutProps)
{
    OutProps.clear();
    if (!Object || !UObjectManager::Get().ContainsObject(Object))
    {
        return;
    }

    TArray<FPropertyDescriptor> Props;
    Object->GetEditableProperties(Props);
    for (const FPropertyDescriptor& Prop : Props)
    {
        if (IsSequencerScalarProperty(Prop))
        {
            OutProps.push_back(Prop);
        }
    }
}

UObject* FEditorActorSequenceEditModel::ResolveBindingObject(
    UActorSequenceComponent* SequenceComp,
    const FActorSequenceBinding& Binding)
{
    return ResolveObject(SequenceComp, Binding);
}

UActorComponent* FEditorActorSequenceEditModel::ResolveBindingComponent(
    UActorSequenceComponent* SequenceComp,
    const FActorSequenceBinding& Binding)
{
    return Cast<UActorComponent>(ResolveObject(SequenceComp, Binding));
}

FString FEditorActorSequenceEditModel::MakeComponentLabel(AActor* Owner, UActorComponent* Component)
{
    if (!Component)
    {
        return "None";
    }

    FString Label = Component->GetFName().ToString();
    if (Label.empty())
    {
        Label = Component->GetTypeInfo() ? Component->GetTypeInfo()->name : "Component";
    }

    if (Owner && Component == Owner->GetRootComponent())
    {
        return "[Root] " + Label;
    }
    return Label;
}

const char* FEditorActorSequenceEditModel::GetDefaultChannelName(EPropertyType Type)
{
    switch (Type)
    {
    case EPropertyType::Vec3:
    case EPropertyType::Vec4:
        return "X";
    case EPropertyType::Color:
        return "R";
    case EPropertyType::Bool:
    case EPropertyType::Int:
    case EPropertyType::Float:
    default:
        return "Value";
    }
}

EActorSequenceTrackType FEditorActorSequenceEditModel::GetTrackType(EPropertyType Type)
{
    switch (Type)
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

void FEditorActorSequenceEditModel::GetChannelNames(EActorSequenceTrackType TrackType, TArray<const char*>& OutNames)
{
    OutNames.clear();
    switch (TrackType)
    {
    case EActorSequenceTrackType::Vec3:
    case EActorSequenceTrackType::Transform:
        OutNames = { "X", "Y", "Z" };
        break;
    case EActorSequenceTrackType::Vec4:
        OutNames = { "X", "Y", "Z", "W" };
        break;
    case EActorSequenceTrackType::Color:
        OutNames = { "R", "G", "B", "A" };
        break;
    case EActorSequenceTrackType::Bool:
    case EActorSequenceTrackType::Int:
    case EActorSequenceTrackType::Float:
    default:
        OutNames = { "Value" };
        break;
    }
}

bool FEditorActorSequenceEditModel::AddTrackForProperty(
    UActorSequenceComponent* SequenceComp,
    UObject* TargetObject,
    const FPropertyDescriptor& Property,
    const char* ChannelName)
{
    if (!IsSequenceComponentLive(SequenceComp)
        || !TargetObject
        || !UObjectManager::Get().ContainsObject(TargetObject)
        || !IsSequencerScalarProperty(Property))
    {
        return false;
    }

    UActorSequence* Sequence = SequenceComp->GetSequence();
    if (!Sequence)
    {
        return false;
    }

    const UActorComponent* TargetComponent = Cast<UActorComponent>(TargetObject);
    if (UActorComponent* MutableTargetComponent = Cast<UActorComponent>(TargetObject))
    {
        MutableTargetComponent->EnsurePersistentGuid();
    }
    const FGuid TargetGuid = TargetComponent ? TargetComponent->GetPersistentGuid() : FGuid();
    const FString TargetName = TargetObject->GetName();
    const FString PropertyPath = Property.Name;
    const EActorSequenceTrackType TrackType = GetTrackType(Property.Type);
    const char* SafeChannelName = ChannelName ? ChannelName : GetDefaultChannelName(Property.Type);

    FActorSequenceBinding* Binding = nullptr;
    for (FActorSequenceBinding& Existing : Sequence->Bindings)
    {
        if ((Existing.Binding.TargetObjectGuid.IsValid() && Existing.Binding.TargetObjectGuid == TargetGuid)
            || (!Existing.Binding.TargetObjectGuid.IsValid() && Existing.Binding.TargetObjectName == TargetName))
        {
            Binding = &Existing;
            break;
        }
    }

    if (!Binding)
    {
        FActorSequenceBinding NewBinding;
        NewBinding.Binding.BindingGuid = FGuid::NewGuid();
        NewBinding.Binding.TargetObjectGuid = TargetGuid;
        NewBinding.Binding.TargetObjectName = TargetName;
        Sequence->Bindings.push_back(NewBinding);
        Binding = &Sequence->Bindings.back();
    }

    FActorSequenceTrack* Track = nullptr;
    for (FActorSequenceTrack& Existing : Binding->Tracks)
    {
        if (Existing.PropertyPath == PropertyPath)
        {
            Track = &Existing;
            break;
        }
    }

    if (!Track)
    {
        FActorSequenceTrack NewTrack;
        NewTrack.TrackGuid = FGuid::NewGuid();
        NewTrack.PropertyPath = PropertyPath;
        NewTrack.TrackType = TrackType;
        Binding->Tracks.push_back(NewTrack);
        Track = &Binding->Tracks.back();
    }

    FActorSequenceSection* Section = Track->Sections.empty() ? nullptr : &Track->Sections.front();
    if (!Section)
    {
        FActorSequenceSection NewSection;
        NewSection.SectionGuid = FGuid::NewGuid();
        NewSection.StartTime = Sequence->StartTime;
        NewSection.Duration = std::max(MinSequenceDuration, Sequence->Duration);
        NewSection.PlayRate = 1.0f;
        Track->Sections.push_back(NewSection);
        Section = &Track->Sections.back();
    }

    if (HasChannel(*Section, SafeChannelName))
    {
        return false;
    }

    FActorSequenceChannel NewChannel;
    NewChannel.ChannelName = SafeChannelName;
    float InitialValue = 0.0f;
    ReadScalarChannelValue(Property, SafeChannelName, InitialValue);
    NewChannel.Playback = MakeDefaultPlayback(InitialValue);
    Section->Channels.push_back(NewChannel);
    return true;
}

bool FEditorActorSequenceEditModel::ResolveChannelByDisplayIndex(
    UActorSequenceComponent* SequenceComp,
    int32 DisplayIndex,
    FActorSequenceChannelHandle& OutHandle)
{
    OutHandle = {};
    if (!IsSequenceComponentLive(SequenceComp) || DisplayIndex < 0)
    {
        return false;
    }

    UActorSequence* Sequence = SequenceComp->GetSequence();
    int32 CurrentIndex = 0;
    for (FActorSequenceBinding& Binding : Sequence->Bindings)
    {
        for (FActorSequenceTrack& Track : Binding.Tracks)
        {
            for (FActorSequenceSection& Section : Track.Sections)
            {
                for (FActorSequenceChannel& Channel : Section.Channels)
                {
                    if (CurrentIndex == DisplayIndex)
                    {
                        OutHandle.Binding = &Binding;
                        OutHandle.Track = &Track;
                        OutHandle.Section = &Section;
                        OutHandle.Channel = &Channel;
                        OutHandle.DisplayIndex = DisplayIndex;
                        return true;
                    }
                    ++CurrentIndex;
                }
            }
        }
    }

    return false;
}

bool FEditorActorSequenceEditModel::AddKeyAtCurrentValue(
    UActorSequenceComponent* SequenceComp,
    const FActorSequenceChannelHandle& Handle,
    float SequenceTime)
{
    if (!IsSequenceComponentLive(SequenceComp)
        || !Handle.Binding
        || !Handle.Track
        || !Handle.Section
        || !Handle.Channel)
    {
        return false;
    }

    FPropertyDescriptor Property;
    if (!ResolveProperty(SequenceComp, *Handle.Binding, *Handle.Track, *Handle.Channel, Property))
    {
        return false;
    }

    float CurrentValue = 0.0f;
    if (!ReadScalarChannelValue(Property, Handle.Channel->ChannelName, CurrentValue))
    {
        return false;
    }

    UCurveFloatAsset* Curve = GetOrCreateChannelCurve(*Handle.Channel);
    if (!Curve)
    {
        return false;
    }

    const float CurveTime = FEditorActorSequenceTimeUtils::SequenceTimeToCurveTime(
        *Handle.Section,
        *Handle.Channel,
        SequenceTime);
    FFloatCurve& FloatCurve = Curve->GetMutableCurve();
    bool bUpdatedExisting = false;
    for (FCurveKey& Key : FloatCurve.Keys)
    {
        if (std::fabs(Key.Time - CurveTime) <= 0.0001f)
        {
            Key.Value = CurrentValue;
            bUpdatedExisting = true;
            break;
        }
    }

    if (!bUpdatedExisting)
    {
        FCurveKey NewKey;
        NewKey.Time = CurveTime;
        NewKey.Value = CurrentValue;
        NewKey.InterpMode = ECurveInterpMode::Cubic;
        NewKey.TangentMode = ECurveTangentMode::Auto;
        FloatCurve.Keys.push_back(NewKey);
    }

    FloatCurve.SortKeys();
    Handle.Channel->Playback.Curve = Curve;
    return true;
}

bool FEditorActorSequenceEditModel::MoveKeyToSequenceTime(
    UActorSequenceComponent* SequenceComp,
    const FActorSequenceChannelHandle& Handle,
    int32 KeyIndex,
    float SequenceTime,
    int32* OutNewKeyIndex)
{
    if (OutNewKeyIndex)
    {
        *OutNewKeyIndex = KeyIndex;
    }

    if (!IsSequenceComponentLive(SequenceComp)
        || !Handle.Section
        || !Handle.Channel
        || KeyIndex < 0)
    {
        return false;
    }

    UCurveFloatAsset* Curve = GetOrCreateChannelCurve(*Handle.Channel);
    if (!Curve)
    {
        return false;
    }

    FFloatCurve& FloatCurve = Curve->GetMutableCurve();
    if (KeyIndex >= static_cast<int32>(FloatCurve.Keys.size()))
    {
        return false;
    }

    const float NewCurveTime = FEditorActorSequenceTimeUtils::SequenceTimeToCurveTime(
        *Handle.Section,
        *Handle.Channel,
        SequenceTime);
    FloatCurve.Keys[KeyIndex].Time = NewCurveTime;
    FloatCurve.SortKeys();
    Handle.Channel->Playback.Curve = Curve;

    int32 NewKeyIndex = KeyIndex;
    float BestDistance = std::numeric_limits<float>::max();
    for (int32 CandidateIndex = 0; CandidateIndex < static_cast<int32>(FloatCurve.Keys.size()); ++CandidateIndex)
    {
        const float Distance = std::fabs(FloatCurve.Keys[CandidateIndex].Time - NewCurveTime);
        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            NewKeyIndex = CandidateIndex;
        }
    }

    if (OutNewKeyIndex)
    {
        *OutNewKeyIndex = NewKeyIndex;
    }
    return true;
}

bool FEditorActorSequenceEditModel::DeleteKeyByIndex(
    UActorSequenceComponent* SequenceComp,
    const FActorSequenceChannelHandle& Handle,
    int32 KeyIndex)
{
    if (!IsSequenceComponentLive(SequenceComp)
        || !Handle.Channel
        || KeyIndex < 0)
    {
        return false;
    }

    UCurveFloatAsset* Curve = GetOrCreateChannelCurve(*Handle.Channel);
    if (!Curve)
    {
        return false;
    }

    FFloatCurve& FloatCurve = Curve->GetMutableCurve();
    if (KeyIndex >= static_cast<int32>(FloatCurve.Keys.size()))
    {
        return false;
    }

    FloatCurve.Keys.erase(FloatCurve.Keys.begin() + KeyIndex);
    FloatCurve.SortKeys();
    Handle.Channel->Playback.Curve = Curve;
    return true;
}

bool FEditorActorSequenceEditModel::DeleteTrackByDisplayIndex(
    UActorSequenceComponent* SequenceComp,
    int32 DisplayIndex)
{
    if (!IsSequenceComponentLive(SequenceComp) || DisplayIndex < 0)
    {
        return false;
    }

    UActorSequence* Sequence = SequenceComp->GetSequence();
    if (!Sequence)
    {
        return false;
    }

    int32 CurrentIndex = 0;
    for (auto BindingIt = Sequence->Bindings.begin(); BindingIt != Sequence->Bindings.end(); ++BindingIt)
    {
        FActorSequenceBinding& Binding = *BindingIt;
        for (auto TrackIt = Binding.Tracks.begin(); TrackIt != Binding.Tracks.end(); ++TrackIt)
        {
            FActorSequenceTrack& Track = *TrackIt;
            for (auto SectionIt = Track.Sections.begin(); SectionIt != Track.Sections.end(); ++SectionIt)
            {
                FActorSequenceSection& Section = *SectionIt;
                for (auto ChannelIt = Section.Channels.begin(); ChannelIt != Section.Channels.end(); ++ChannelIt)
                {
                    if (CurrentIndex == DisplayIndex)
                    {
                        Section.Channels.erase(ChannelIt);
                        if (Section.Channels.empty())
                        {
                            Track.Sections.erase(SectionIt);
                        }
                        if (Track.Sections.empty())
                        {
                            Binding.Tracks.erase(TrackIt);
                        }
                        if (Binding.Tracks.empty())
                        {
                            Sequence->Bindings.erase(BindingIt);
                        }
                        return true;
                    }
                    ++CurrentIndex;
                }
            }
        }
    }

    return false;
}

bool FEditorActorSequenceEditModel::SetApplyModeByDisplayIndex(
    UActorSequenceComponent* SequenceComp,
    int32 DisplayIndex,
    ECurveApplyMode ApplyMode)
{
    FActorSequenceChannelHandle Handle;
    if (!ResolveChannelByDisplayIndex(SequenceComp, DisplayIndex, Handle) || !Handle.Channel)
    {
        return false;
    }

    if (Handle.Channel->Playback.ApplyMode == ApplyMode)
    {
        return false;
    }

    Handle.Channel->Playback.ApplyMode = ApplyMode;
    return true;
}

bool FEditorActorSequenceEditModel::SetTimeMappingModeByDisplayIndex(
    UActorSequenceComponent* SequenceComp,
    int32 DisplayIndex,
    ECurveTimeMappingMode TimeMappingMode)
{
    FActorSequenceChannelHandle Handle;
    if (!ResolveChannelByDisplayIndex(SequenceComp, DisplayIndex, Handle) || !Handle.Channel)
    {
        return false;
    }

    if (Handle.Channel->Playback.TimeMappingMode == TimeMappingMode)
    {
        return false;
    }

    Handle.Channel->Playback.TimeMappingMode = TimeMappingMode;
    return true;
}

bool FEditorActorSequenceEditModel::RemoveKeyNearTime(
    UActorSequenceComponent* SequenceComp,
    const FActorSequenceChannelHandle& Handle,
    float SequenceTime,
    float SequenceTolerance)
{
    if (!IsSequenceComponentLive(SequenceComp)
        || !Handle.Section
        || !Handle.Channel)
    {
        return false;
    }

    UCurveFloatAsset* Curve = GetOrCreateChannelCurve(*Handle.Channel);
    if (!Curve)
    {
        return false;
    }

    const float CurveTime = FEditorActorSequenceTimeUtils::SequenceTimeToCurveTime(
        *Handle.Section,
        *Handle.Channel,
        SequenceTime);
    const float CurveTolerance = std::max(
        0.001f,
        FEditorActorSequenceTimeUtils::SequenceTimeToCurveTime(
            *Handle.Section,
            *Handle.Channel,
            SequenceTime + SequenceTolerance) - CurveTime);
    FFloatCurve& FloatCurve = Curve->GetMutableCurve();

    int32 RemoveIndex = -1;
    float BestDistance = CurveTolerance;
    for (int32 KeyIndex = 0; KeyIndex < static_cast<int32>(FloatCurve.Keys.size()); ++KeyIndex)
    {
        const float Distance = std::fabs(FloatCurve.Keys[KeyIndex].Time - CurveTime);
        if (Distance <= BestDistance)
        {
            BestDistance = Distance;
            RemoveIndex = KeyIndex;
        }
    }

    if (RemoveIndex < 0)
    {
        return false;
    }

    FloatCurve.Keys.erase(FloatCurve.Keys.begin() + RemoveIndex);
    FloatCurve.SortKeys();
    Handle.Channel->Playback.Curve = Curve;
    return true;
}

bool FEditorActorSequenceEditModel::ResizeSection(
    FActorSequenceSection& Section,
    float SequenceTime,
    bool bResizeStart,
    float MinDuration)
{
    const float SafeMinDuration = std::max(0.001f, MinDuration);
    const float SectionEnd = Section.StartTime + std::max(0.0f, Section.Duration);
    if (bResizeStart)
    {
        const float NewStart = std::min(SequenceTime, SectionEnd - SafeMinDuration);
        Section.StartTime = NewStart;
        Section.Duration = std::max(SafeMinDuration, SectionEnd - NewStart);
        return true;
    }

    const float NewEnd = std::max(SequenceTime, Section.StartTime + SafeMinDuration);
    Section.Duration = std::max(SafeMinDuration, NewEnd - Section.StartTime);
    return true;
}

bool FEditorActorSequenceEditModel::ResizePlaybackRange(
    UActorSequence& Sequence,
    float SequenceTime,
    bool bResizeStart,
    float MinDuration)
{
    const float SafeMinDuration = std::max(0.001f, MinDuration);
    const float PlaybackEnd = Sequence.StartTime + std::max(0.0f, Sequence.Duration);
    if (bResizeStart)
    {
        const float NewStartTime = std::min(SequenceTime, PlaybackEnd - SafeMinDuration);
        Sequence.StartTime = NewStartTime;
        Sequence.Duration = std::max(SafeMinDuration, PlaybackEnd - NewStartTime);
        return true;
    }

    const float NewEndTime = std::max(SequenceTime, Sequence.StartTime + SafeMinDuration);
    Sequence.Duration = std::max(SafeMinDuration, NewEndTime - Sequence.StartTime);
    return true;
}

UCurveFloatAsset* FEditorActorSequenceEditModel::GetOrCreateChannelCurve(FActorSequenceChannel& Channel)
{
    if (!Channel.Playback.Curve && !Channel.Playback.CurveAssetPath.empty())
    {
        UCurveFloatAsset* SourceCurve = FResourceManager::Get().LoadCurve(Channel.Playback.CurveAssetPath);
        if (SourceCurve)
        {
            UCurveFloatAsset* InlineCurve = UObjectManager::Get().CreateObject<UCurveFloatAsset>();
            if (InlineCurve)
            {
                InlineCurve->GetMutableCurve() = SourceCurve->GetCurve();
                Channel.Playback.Curve = InlineCurve;
                Channel.Playback.CurveAssetPath.clear();
            }
        }
    }
    if (!Channel.Playback.Curve)
    {
        Channel.Playback.Curve = CreateSequenceOwnedCurve(0.0f);
    }
    return Channel.Playback.Curve;
}

bool FEditorActorSequenceEditModel::CaptureSequenceUndo(
    UEditorEngine* EditorEngine,
    const char* UndoLabel)
{
    if (!EditorEngine)
    {
        return false;
    }

    return EditorEngine->GetUndoSystem().CaptureSnapshot(UndoLabel ? UndoLabel : "Edit Actor Sequence");
}

void FEditorActorSequenceEditModel::NotifySequenceEdited(
    UEditorEngine* EditorEngine,
    UActorSequenceComponent* SequenceComp,
    const char* UndoLabel,
    bool bCaptureUndo)
{
    if (!IsSequenceComponentLive(SequenceComp))
    {
        return;
    }

    if (bCaptureUndo)
    {
        CaptureSequenceUndo(EditorEngine, UndoLabel);
    }

    SequenceComp->MarkSequenceDirty();
    SequenceComp->PostEditChangeProperty({ "Sequence", EPropertyChangeType::ValueSet });

    if (EditorEngine)
    {
        EditorEngine->GetSceneService().MarkDirty();
    }
}
