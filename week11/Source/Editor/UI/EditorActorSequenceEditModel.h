#pragma once

#include "Animation/ActorSequence.h"
#include "Core/CoreMinimal.h"
#include "Core/PropertyTypes.h"

class AActor;
class UObject;
class UActorComponent;
class UActorSequenceComponent;
class UCurveFloatAsset;
class UEditorEngine;

struct FActorSequenceChannelHandle
{
    FActorSequenceBinding* Binding = nullptr;
    FActorSequenceTrack* Track = nullptr;
    FActorSequenceSection* Section = nullptr;
    FActorSequenceChannel* Channel = nullptr;
    int32 DisplayIndex = -1;
};

class FEditorActorSequenceEditModel
{
public:
    static bool IsSequenceComponentLive(UActorSequenceComponent* SequenceComp);
    static AActor* GetLiveOwner(UActorSequenceComponent* SequenceComp);

    static void CollectAnimatableScalarProperties(
        UObject* Object,
        TArray<FPropertyDescriptor>& OutProps);
    static UObject* ResolveBindingObject(
        UActorSequenceComponent* SequenceComp,
        const FActorSequenceBinding& Binding);
    static UActorComponent* ResolveBindingComponent(
        UActorSequenceComponent* SequenceComp,
        const FActorSequenceBinding& Binding);

    static FString MakeComponentLabel(AActor* Owner, UActorComponent* Component);
    static const char* GetDefaultChannelName(EPropertyType Type);
    static EActorSequenceTrackType GetTrackType(EPropertyType Type);
    static void GetChannelNames(EActorSequenceTrackType TrackType, TArray<const char*>& OutNames);

    static bool AddTrackForProperty(
        UActorSequenceComponent* SequenceComp,
        UObject* TargetObject,
        const FPropertyDescriptor& Property,
        const char* ChannelName);

    static bool ResolveChannelByDisplayIndex(
        UActorSequenceComponent* SequenceComp,
        int32 DisplayIndex,
        FActorSequenceChannelHandle& OutHandle);

    static bool AddKeyAtCurrentValue(
        UActorSequenceComponent* SequenceComp,
        const FActorSequenceChannelHandle& Handle,
        float SequenceTime);
    static bool MoveKeyToSequenceTime(
        UActorSequenceComponent* SequenceComp,
        const FActorSequenceChannelHandle& Handle,
        int32 KeyIndex,
        float SequenceTime,
        int32* OutNewKeyIndex = nullptr);
    static bool DeleteKeyByIndex(
        UActorSequenceComponent* SequenceComp,
        const FActorSequenceChannelHandle& Handle,
        int32 KeyIndex);
    static bool DeleteTrackByDisplayIndex(
        UActorSequenceComponent* SequenceComp,
        int32 DisplayIndex);
    static bool SetApplyModeByDisplayIndex(
        UActorSequenceComponent* SequenceComp,
        int32 DisplayIndex,
        ECurveApplyMode ApplyMode);
    static bool SetTimeMappingModeByDisplayIndex(
        UActorSequenceComponent* SequenceComp,
        int32 DisplayIndex,
        ECurveTimeMappingMode TimeMappingMode);
    static bool RemoveKeyNearTime(
        UActorSequenceComponent* SequenceComp,
        const FActorSequenceChannelHandle& Handle,
        float SequenceTime,
        float SequenceTolerance);
    static bool ResizeSection(
        FActorSequenceSection& Section,
        float SequenceTime,
        bool bResizeStart,
        float MinDuration);
    static bool ResizePlaybackRange(
        UActorSequence& Sequence,
        float SequenceTime,
        bool bResizeStart,
        float MinDuration);
    static UCurveFloatAsset* GetOrCreateChannelCurve(FActorSequenceChannel& Channel);

    static bool CaptureSequenceUndo(
        UEditorEngine* EditorEngine,
        const char* UndoLabel);

    static void NotifySequenceEdited(
        UEditorEngine* EditorEngine,
        UActorSequenceComponent* SequenceComp,
        const char* UndoLabel,
        bool bCaptureUndo = true);
};
