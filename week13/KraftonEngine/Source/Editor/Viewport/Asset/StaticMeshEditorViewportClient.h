#pragma once

#include "Viewport/EditorPreviewViewportClient.h"
#include "Viewport/ViewportClient.h"
#include "Editor/Viewport/ViewportCameraTransform.h"
#include "Editor/Slate/SWindow.h"
#include "Core/Types/RayTypes.h"
#include "Gizmo/GizmoTransformTarget.h"
#include "PhysicsEngine/ShapeElem.h"
#include "Object/GarbageCollection.h"
#include "Object/Ptr/WeakObjectPtr.h"

#include <d3d11.h>

class FWindowsWindow;
class UGizmoComponent;
class UStaticMeshComponent;
class UWorld;
class AActor;

struct FBodySetupShapeSelection
{
	EAggCollisionShape Type = EAggCollisionShape::Unknown;
	int32 Index = -1;

	bool IsValid() const
	{
		return Type != EAggCollisionShape::Unknown && Index >= 0;
	}

	bool operator==(const FBodySetupShapeSelection& Other) const
	{
		return Type == Other.Type && Index == Other.Index;
	}

	bool operator!=(const FBodySetupShapeSelection& Other) const
	{
		return !(*this == Other);
	}
};

class FBodySetupShapeGizmoTarget : public IGizmoTransformTarget
{
public:
	void SetShape(UStaticMeshComponent* InComponent, FBodySetupShapeSelection InSelection);
	void Clear();
	bool IsValid() const;

	UWorld* GetWorld() const override;
	FVector GetWorldLocation() const override;
	FRotator GetWorldRotation() const override;
	FQuat GetWorldQuat() const override;
	FVector GetWorldScale() const override;
	void SetWorldLocation(const FVector& NewLocation) override;
	void SetWorldRotation(const FRotator& NewRotation) override;
	void SetWorldRotation(const FQuat& NewQuat) override;
	void SetWorldScale(const FVector& NewScale) override;
	void AddWorldOffset(const FVector& Delta) override;
	void AddWorldRotation(const FQuat& Delta, bool bWorldSpace) override;
	void AddScaleDelta(const FVector& Delta) override;

private:
	struct FEditableShape;

	UStaticMeshComponent* GetComponent() const;
	bool GetEditableShape(FEditableShape& OutShape) const;
	bool GetComponentEditTransform(FTransform& OutComponentTM, FVector& OutScale3D) const;
	FTransform GetShapeWorldTransform() const;
	void MarkShapeChanged() const;

private:
	TWeakObjectPtr<UStaticMeshComponent> Component;
	FBodySetupShapeSelection Selection;
};

class FStaticMeshEditorViewportClient : public FViewportClient, public IEditorPreviewViewportClient, public FGCObject
{
public:
	void Initialize(ID3D11Device* Device, uint32 Width, uint32 Height);
	void Release();

	const char* GetReferencerName() const override { return "FStaticMeshEditorViewportClient"; }
	void AddReferencedObjects(FReferenceCollector& Collector) override;

	void CreatePreviewGizmo();
	void ResetCameraToPreviewBounds();

	void SetPreviewWorld(UWorld* InWorld) { PreviewWorld = InWorld; }
	void SetPreviewActor(AActor* InActor) { PreviewActor = InActor; }
	void SetPreviewMeshComponent(UStaticMeshComponent* InComp) { PreviewMeshComponent = InComp; }
	void SetViewportRect(float X, float Y, float Width, float Height) { ViewportScreenRect = { X, Y, Width, Height }; }

	bool IsRenderable() const override { return bIsRenderable; }
	bool IsMouseOverViewport() const override;

	FViewport* GetViewport() const override { return Viewport; }
	UWorld* GetPreviewWorld() const override { return PreviewWorld; }

	FViewportRenderOptions& GetRenderOptions() override { return RenderOptions; }
	const FViewportRenderOptions& GetRenderOptions() const override { return RenderOptions; }
	UGizmoComponent* GetGizmo() const { return Gizmo; }

	void NotifyViewportResized(int32 NewWidth, int32 NewHeight) override;
	bool GetCameraView(FMinimalViewInfo& OutPOV) const override;

	void Tick(float DeltaTime);
	void ApplyTransformSettingsToGizmo();
	void SetBodySetupEditingEnabled(bool bInEnabled);
	void SetSelectedBodySetupShape(FBodySetupShapeSelection InSelection);
	FBodySetupShapeSelection GetSelectedBodySetupShape() const { return SelectedBodySetupShape; }
	void MarkBodySetupDebugDirty();
	void SetOnBodySetupShapePicked(TFunction<void(FBodySetupShapeSelection)> InCallback);
	void SetOnBodySetupShapeEdited(TFunction<void()> InCallback);

private:
	struct FBodySetupShapeHitResult;

	void TickShortcuts();
	void TickInput(float DeltaTime);
	void TickInteraction(float DeltaTime);
	void SyncCameraSmoothingTarget();
	void ApplySmoothedCameraLocation(float DeltaTime);
	void SyncBodySetupShapeGizmoTarget();
	void NotifyBodySetupShapePicked(FBodySetupShapeSelection InSelection);
	void HandleDragStart(const FRay& Ray);
	bool PickBodySetupShape(const FRay& Ray, FBodySetupShapeHitResult& OutHit) const;
	bool IsBodySetupShapeGizmoActive() const;

private:
	FViewport* Viewport = nullptr;
	FWindowsWindow* Window = nullptr;
	FViewportRenderOptions RenderOptions;

	UWorld* PreviewWorld = nullptr;
	AActor* PreviewActor = nullptr;
	UStaticMeshComponent* PreviewMeshComponent = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	FBodySetupShapeGizmoTarget BodySetupShapeTarget;
	bool bBodySetupEditingEnabled = false;
	FBodySetupShapeSelection SelectedBodySetupShape;
	TFunction<void(FBodySetupShapeSelection)> OnBodySetupShapePicked;
	TFunction<void()> OnBodySetupShapeEdited;
	uint64 BodySetupDebugRevision = 0;

	bool bIsRenderable = false;

	FViewportCameraTransform ViewTransform;
	FRect ViewportScreenRect;

	FVector TargetLocation;
	bool bTargetLocationInitialized = false;
	FVector LastAppliedCameraLocation;
	bool bLastAppliedCameraLocationInitialized = false;
	const float SmoothLocationSpeed = 10.0f;
};
