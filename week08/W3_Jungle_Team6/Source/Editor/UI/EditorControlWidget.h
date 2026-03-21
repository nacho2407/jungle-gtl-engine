#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Math/Vector.h"

enum class EPrimitiveType;

class FEditorControlWidget : public FEditorWidget
{
public:
	virtual void Initialize(FEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime, FViewOutput& ViewOutput) override;

private:
	const char* PrimitiveTypes[4] = { "Cube", "Sphere", "Plane", "Quad"};
	int32 SelectedPrimitiveType = 0;
	int32 NumberOfSpawnedActors = 1;
	FVector CurSpawnPoint = { 0.f, 0.f, 0.f };
};
