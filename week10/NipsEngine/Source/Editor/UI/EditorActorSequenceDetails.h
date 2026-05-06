#pragma once

#include "Core/CoreMinimal.h"

class AActor;
class UActorComponent;
class UActorSequenceComponent;
class UEditorEngine;
struct FActorSequenceBinding;
struct FActorSequenceChannel;
struct FActorSequenceSection;
struct FActorSequenceTrack;
struct FPropertyDescriptor;

class FEditorActorSequenceDetails
{
public:
	void Initialize(UEditorEngine* InEditorEngine, bool* InUndoCaptureFlag);
	void Render(UActorSequenceComponent* SequenceComp, float DeltaTime);

private:
	void CollectAnimatableScalarProperties(UActorComponent* Component, TArray<FPropertyDescriptor>& OutProps) const;
	UActorComponent* ResolveTrackComponent(UActorSequenceComponent* SequenceComp, const FActorSequenceBinding& Binding) const;
	FString MakeComponentLabel(AActor* Owner, UActorComponent* Component) const;
	void MarkEdited(UActorSequenceComponent* SequenceComp, const char* UndoLabel);

	bool AddTrack(UActorSequenceComponent* SequenceComp, bool bAddAllChannels);
	bool AddMissingChannelsToSequence(UActorSequenceComponent* SequenceComp);
	bool RenderTrack(
		UActorSequenceComponent* SequenceComp,
		FActorSequenceBinding& Binding,
		FActorSequenceTrack& Track,
		FActorSequenceSection& Section,
		FActorSequenceChannel& Channel,
		int32 TrackIndex);

private:
	UEditorEngine* EditorEngine = nullptr;
	bool* UndoCaptureFlag = nullptr;
};
