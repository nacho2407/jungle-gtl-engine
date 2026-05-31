#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Core/Types/CoreTypes.h"

class FEditorSceneWidget : public FEditorWidget
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;
	void SetShowEditorOnlyComponents(bool bEnable) { bShowEditorOnlyComponents = bEnable; }
	bool IsShowingEditorOnlyComponents() const { return bShowEditorOnlyComponents; }

private:
	void RenderActorOutliner();
	void RenderActorNode(class AActor* Actor, const TArray<AActor*>& Actors);
	void RenderSceneComponentNode(class USceneComponent* Comp);
	void RenderNonSceneComponents(class AActor* Actor);

	TArray<int32> ValidActorIndices;
	bool bShowEditorOnlyComponents = false;
};
