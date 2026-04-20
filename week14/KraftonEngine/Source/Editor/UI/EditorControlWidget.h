#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Math/Vector.h"

class FEditorControlWidget : public FEditorWidget
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;

private:
	const char* PrimitiveTypes[10] = { "Cube", "Sphere", "Decal", "Height Fog", "Fake Light",
		"Fireball","AmbientLight","DirectionalLight","PointLight","SpotLight" };
	int32 SelectedPrimitiveType = 0;
	int32 NumberOfSpawnedActors = 1;
	FVector CurSpawnPoint = { 0.f, 0.f, 0.f };

	// Grid Spawn
	float GridCenterOffset[3] = { 0.f, 0.f, 0.f };
	int32 GridCountX = 1;
	int32 GridCountY = 1;
	float GridSpacingX = 5.f;
	float GridSpacingY = 5.f;
};
