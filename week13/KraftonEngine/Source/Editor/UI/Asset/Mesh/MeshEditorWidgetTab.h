#pragma once

#include "MeshEditorWidget.h"

struct ImDrawList;
struct ImVec2;
class AActor;
class UObject;
class USkeletalMesh;

class FMeshEditorWidgetTab
{
public:
	explicit FMeshEditorWidgetTab(FMeshEditorWidget& InOwner);
	virtual ~FMeshEditorWidgetTab() = default;

	virtual EMeshEditorTab GetType() const = 0;
	virtual const char* GetLabel() const = 0;
	virtual const wchar_t* GetIconFileName() const = 0;

	virtual bool CanEdit(UObject* Object) const { return false; }
	virtual bool IsEditingObject(UObject* Object) const { return false; }
	virtual bool ShouldActivateOnReuse(UObject* Object) const { return false; }
	virtual bool ResolveOpenTarget(UObject* Object, UObject*& OutObjectToEdit, EMeshEditorTab& OutInitialTab) const { return false; }
	virtual FString GetEditorTitleAssetPath() const;

	virtual void Render(float AvailableHeight) = 0;
	virtual void Tick(float DeltaTime) {}
	virtual void Reset() {}
	virtual void OnPreviewActorCreated(AActor* Actor) {}
	virtual void OnEditorOpened() {}
	virtual void OnEditorClosing() {}
	virtual void OnInitialActivated() { OnActivated(GetType()); }
	virtual void OnActivated(EMeshEditorTab PreviousTab) {}
	virtual void OnDeactivated(EMeshEditorTab NextTab) {}

	virtual void OnPhysicsAssetBodyPicked(int32 BodyIndex) {}
	virtual void OnPhysicsAssetConstraintPicked(int32 ConstraintIndex) {}
	virtual void OnPhysicsAssetShapeEdited() {}
	virtual void OnPhysicsAssetConstraintEdited() {}

protected:
	UObject* GetEditedObject() const;
	USkeletalMesh* GetSkeletalMesh() const;
	FMeshEditorViewportClient& GetViewportClient();
	const FMeshEditorViewportClient& GetViewportClient() const;
	uint32 GetOwnerInstanceId() const;

	void MarkDirty();
	bool IsEditingCurrentSkeletalMesh(UObject* Object) const;
	void RenderViewportPanel(ImVec2 Size);
	void RenderMeshStatsOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos) const;
	virtual int32 GetSelectedBoneIndexForViewport() const { return -1; }

protected:
	FMeshEditorWidget& Owner;
};
