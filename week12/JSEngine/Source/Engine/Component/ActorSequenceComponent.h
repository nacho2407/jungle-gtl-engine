#pragma once

#include "Animation/ActorSequence.h"
#include "Component/ActorComponent.h"

UCLASS(SpawnableComponent, DisplayName = "ActorSequence Component", Category = "System")
class UActorSequenceComponent : public UActorComponent
{
public:
	GENERATED_BODY(UActorSequenceComponent, UActorComponent)

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

	bool IsAutoPlay() const { return bAutoPlay; }
	void SetAutoPlay(bool bInAutoPlay) { bAutoPlay = bInAutoPlay; }
	bool IsLooping() const;
	void SetLooping(bool bInLooping);
	float GetPlayRate() const { return PlayRate; }
	void SetPlayRate(float InPlayRate);
	bool ShouldPauseAtEnd() const { return bPauseAtEnd; }
	void SetPauseAtEnd(bool bInPauseAtEnd);
	float GetStartOffsetSeconds() const { return StartOffsetSeconds; }
	void SetStartOffsetSeconds(float InStartOffsetSeconds);

	void MarkSequenceDirty();

	void Serialize(FArchive& Ar) override;
	void PostDuplicate(UObject* Original) override;

protected:
	void TickComponent(float DeltaTime) override;

private:
	void EnsureSequence();
	void EnsureSequencePlayer(ESequencePlayerContext Context);
	void EnsurePreviewSequencePlayer();
	void ApplyPlaybackSettings(UActorSequencePlayer* Player);

private:
	UActorSequence* Sequence = nullptr;
	UActorSequencePlayer* SequencePlayer = nullptr;
	UActorSequencePlayer* PreviewSequencePlayer = nullptr;

	UPROPERTY(DisplayName = "Auto Play")
	bool bAutoPlay = true;

	UPROPERTY(DisplayName = "Pause At End")
	bool bPauseAtEnd = false;

	UPROPERTY(DisplayName = "Play Rate", ClampMin = 0.0, Speed = 0.05)
	float PlayRate = 1.0f;

	UPROPERTY(DisplayName = "Start Offset Seconds", ClampMin = 0.0, Speed = 0.05)
	float StartOffsetSeconds = 0.0f;
};
