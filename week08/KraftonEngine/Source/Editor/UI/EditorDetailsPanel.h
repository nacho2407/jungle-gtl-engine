#pragma once

#include "Editor/UI/EditorPanel.h"
#include "Object/Object.h"

class UActorComponent;
class AActor;

class FEditorDetailsPanel : public FEditorPanel
{
public:
	virtual void Render(float DeltaTime) override;

	UActorComponent* GetSelectedComponent() const { return SelectedComponent; }

private:
	void RenderComponentTree(AActor* Actor);
	void RenderSceneComponentNode(class USceneComponent* Comp);
	void RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	void RenderComponentProperties(AActor* Actor);
	void RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	bool RenderDetailsPanel(TArray<struct FPropertyDescriptor>& Props, int32& Index);

	static FString OpenObjFileDialog();

	UActorComponent* SelectedComponent = nullptr;
	AActor* LastSelectedActor = nullptr;
	bool bActorSelected = true; // true: Actor details, false: Component details
};
