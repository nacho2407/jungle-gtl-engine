#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Object/Object.h"

class FSelectionManager;
class UActorComponent;
class AActor;

class FEditorPropertyWidget : public FEditorWidget
{
public:
	virtual void Render(float DeltaTime) override;
	void Initialize(UEditorEngine* InEditorEngine) override;

private:
	void RenderComponentTree(AActor* Actor);
	void RenderSceneComponentNode(class USceneComponent* Comp, UActorComponent*& OutCompToDelete);
	void RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	void RenderComponentProperties();
	void RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	void RenderPropertyWidget(struct FPropertyDescriptor& Prop);

	FSelectionManager* SelectionManager = nullptr;
	UActorComponent* SelectedComponent = nullptr;
	AActor* LastSelectedActor = nullptr;
	bool bActorSelected = true; // true: Actor details, false: Component details
};
