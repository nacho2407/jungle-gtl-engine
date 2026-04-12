#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Math/Vector.h"

class FEditorControlWidget : public FEditorWidget
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;

private:
	const char* PrimitiveTypes[7] = { "StaticMesh", "TextRender", "SubUV" , "Billboard", "Decal", "Fireball", "Spotlight" };
	int32 SelectedPrimitiveType = 0;
	int32 NumberOfSpawnedActors = 1;
	FVector CurSpawnPoint = { 0.f, 0.f, 0.f };
};
