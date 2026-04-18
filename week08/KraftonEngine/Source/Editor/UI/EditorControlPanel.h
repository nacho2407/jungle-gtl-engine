#pragma once

#include "Editor/UI/EditorPanel.h"
#include "Math/Vector.h"

class FEditorControlPanel : public FEditorPanel
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;

private:
	int32 SelectedPrimitiveType = 0;
	int32 NumberOfSpawnedActors = 1;
	FVector CurSpawnPoint = { 0.f, 0.f, 0.f };
};
