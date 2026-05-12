#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Core/Guid.h"

class UActorSequenceComponent;
class UActorComponent;
struct FActorSequenceBinding;
struct FActorSequenceChannel;
struct FActorSequenceSection;
struct FActorSequenceTrack;

class FEditorActorSequencerWidget : public FEditorWidget
{
public:
    void Render(float DeltaTime) override;

    void Open(UActorSequenceComponent* InSequenceComponent);
    void ResetTarget();
    bool IsVisible() const { return bVisible; }

private:
    FString MakeTrackLabel(
        const FActorSequenceTrack& Track,
        const FActorSequenceChannel& Channel) const;
    void DrawToolbar(UActorSequenceComponent* SequenceComp);
    void DrawAddTrackPopup(UActorSequenceComponent* SequenceComp);
    void DrawAddPropertyPopup(UActorSequenceComponent* SequenceComp);
    void AddKeyToSelectedTrack(UActorSequenceComponent* SequenceComp);
    void DrawSequencer(UActorSequenceComponent* SequenceComp);
    bool IsComponentPinned(const FGuid& Guid) const;
    void PinComponent(UActorComponent* Component);

private:
    UActorSequenceComponent* SequenceComponent = nullptr;
    bool bVisible = false;
    float ViewStartTime = -0.5f;
    float ViewEndTime = 5.5f;
    int32 SelectedTrackIndex = -1;
    int32 SelectedKeyTrackIndex = -1;
    int32 SelectedKeyIndex = -1;
    bool bDraggingPlaybackStart = false;
    bool bDraggingPlaybackEnd = false;
    bool bDraggingSectionStart = false;
    bool bDraggingSectionEnd = false;
    int32 DraggingSectionTrackIndex = -1;
    bool bDraggingKey = false;
    int32 DraggingKeyTrackIndex = -1;
    int32 DraggingKeyIndex = -1;
    int32 ContextTrackIndex = -1;
    float ContextSequenceTime = 0.0f;
    FGuid PendingAddPropertyComponentGuid;
    TArray<FGuid> PinnedComponentGuids;
};
