#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Math/Vector.h"

class FEditorControlWidget : public FEditorWidget
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;
	bool SpawnPrimitive(int32 PrimitiveType, const FVector& SpawnPoint, int32 Count = 1);
	bool DrawPlaceActorMenu(const FVector& SpawnPoint, bool bClosePopupOnSpawn = false);
	const char* GetPrimitiveTypeLabel(int32 PrimitiveType) const;
	int32 GetPrimitiveTypeCount() const { return 13; }

private:
    const char* PrimitiveTypes[13] = { "Scene",
                                       "StaticMesh",
                                       "TextRender",
                                       "SubUV",
                                       "Billboard",
                                       "Decal",
                                       "Fireball",
                                       "DecalSpotlight",
                                       "Ambient",
                                       "Directional",
                                       "Point",
                                       "Spotlight",
                                       "Fog" };
	int32 SelectedPrimitiveType = 0;
	int32 NumberOfSpawnedActors = 1;
	FVector CurSpawnPoint = { 0.f, 0.f, 0.f };
};
