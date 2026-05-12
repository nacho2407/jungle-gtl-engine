#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Object/Object.h"

class UActorComponent;
class AActor;

class FEditorPropertyWidget : public FEditorWidget
{
public:
	virtual void Render(float DeltaTime) override;
	void SetShowEditorOnlyComponents(bool bEnable) { bShowEditorOnlyComponents = bEnable; }
	bool IsShowingEditorOnlyComponents() const { return bShowEditorOnlyComponents; }

private:
	void RenameActor(AActor* PrimaryActor);
	void RenderComponentTree(AActor* Actor);
	void RenderSceneComponentNode(class USceneComponent* Comp);
	void RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	void RenderComponentProperties(AActor* Actor, const TArray<AActor*>& SelectedActors);
	void RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	bool RenderPropertyWidget(TArray<struct FPropertyDescriptor>& Props, int32& Index);

	void PropagatePropertyChange(const FString& PropName, const TArray<AActor*>& SelectedActors);

	void AddComponentToActor(AActor* Actor, UClass* ComponentClass);

	static FString OpenObjFileDialog();
	static FString OpenStaticMeshFileDialog();
	static FString OpenFbxFileDialog();

	UActorComponent* SelectedComponent = nullptr;
	AActor* LastSelectedActor = nullptr;
	bool bActorSelected = true; // true: Actor details, false: Component details
	bool bShowEditorOnlyComponents = false;

	char RenameBuffer[256] = {};
	bool bShowDuplicateWarning = false;
	FString PendingStaticMeshImportPath;
	FString* PendingStaticMeshImportTarget = nullptr;
	int32 PendingStaticFbxSkinnedMeshPolicy = 0;
};
