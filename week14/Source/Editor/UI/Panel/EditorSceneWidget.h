#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"

class FEditorSceneWidget : public FEditorWidget
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;

private:
	void RenderActorOutliner();
	void RenderActorRow(class AActor* Actor, const TArray<class AActor*>& Actors);
	void RenderGroupRow(struct FSceneOutlinerGroup& Group, const TArray<class AActor*>& Actors);
	void RenderOutlinerContextMenu();
	void RenderRenameGroupPopup();
	void GroupSelectedActors();
	void CreatePrefabFromSelectedActors();
	void InstantiatePrefabFromContentItem(const struct FContentItem& ContentItem);
	class AActor* ResolveActorByUUID(uint32 ActorUUID) const;
	TArray<class AActor*> GetSelectedActors() const;
	FVector GetDefaultPrefabPlacementLocation() const;

	TArray<int32> ValidActorIndices;
	uint32 PendingRenameGroupId = 0;
	char RenameGroupBuffer[128] = {};
};
