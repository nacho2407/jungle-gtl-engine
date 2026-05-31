#pragma once

#include "Editor/UI/Panel/EditorPanelWidget.h"
#include "Core/Types/CoreTypes.h"

class FEditorSceneWidget : public FEditorPanelWidget
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(const FEditorPanelContext& Context) override;
	void SetShowEditorOnlyComponents(bool bEnable) { bShowEditorOnlyComponents = bEnable; }
	bool IsShowingEditorOnlyComponents() const { return bShowEditorOnlyComponents; }

private:
	void RenderActorOutliner(class FSelectionManager& Selection);
	void RenderActorNode(class AActor* Actor, const TArray<AActor*>& Actors, class FSelectionManager& Selection);
	void RenderSceneComponentNode(class USceneComponent* Comp, class FSelectionManager& Selection);
	void RenderNonSceneComponents(class AActor* Actor, class FSelectionManager& Selection);

	TArray<int32> ValidActorIndices;
	bool bShowEditorOnlyComponents = false;
};
