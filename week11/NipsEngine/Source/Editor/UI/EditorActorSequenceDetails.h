#pragma once

#include "Core/CoreMinimal.h"

class UActorSequenceComponent;
class UEditorEngine;

class FEditorActorSequenceDetails
{
public:
	void Initialize(UEditorEngine* InEditorEngine, bool* InUndoCaptureFlag);
	void Render(UActorSequenceComponent* SequenceComp, float DeltaTime);

private:
	void MarkEdited(UActorSequenceComponent* SequenceComp, const char* UndoLabel);

private:
	UEditorEngine* EditorEngine = nullptr;
	bool* UndoCaptureFlag = nullptr;
};
