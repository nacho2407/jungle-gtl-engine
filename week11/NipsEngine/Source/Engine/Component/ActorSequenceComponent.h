#pragma once

#include "Animation/ActorSequence.h"
#include "Component/ActorComponent.h"

class UActorSequenceComponent : public UActorComponent
{
public:
    DECLARE_CLASS(UActorSequenceComponent, UActorComponent)

    void BeginPlay() override;
    void EndPlay() override;
    void ExecutePreviewTick(float DeltaTime);

    void Play();
    void Pause();
    void Stop();
    void PlayPreview();
    void PausePreview();
    void StopPreview();
    void SetPreviewTime(float InTime);
    bool AddFloatTrack(const FActorSequenceFloatTrackDesc& Desc);

    UActorSequence* GetSequence();
    UActorSequencePlayer* GetSequencePlayer();
    UActorSequencePlayer* GetPreviewSequencePlayer();
    bool ShouldRestoreOnStop() const { return bRestoreOnStop; }
    void SetRestoreOnStop(bool bInRestoreOnStop) { bRestoreOnStop = bInRestoreOnStop; }

    void MarkSequenceDirty();

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void Serialize(FArchive& Ar) override;
    void PostDuplicate(UObject* Original) override;

protected:
    void TickComponent(float DeltaTime) override;

private:
    void EnsureSequence();
    void EnsureSequencePlayer(ESequencePlayerContext Context);
    void EnsurePreviewSequencePlayer();

private:
    UActorSequence* Sequence = nullptr;
    UActorSequencePlayer* SequencePlayer = nullptr;
    UActorSequencePlayer* PreviewSequencePlayer = nullptr;

    bool bAutoPlay = true;
    bool bRestoreOnStop = true;
};
