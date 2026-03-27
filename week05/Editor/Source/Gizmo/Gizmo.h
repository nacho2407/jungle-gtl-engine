#pragma once

#include "CoreMinimal.h"
#include "Primitive/PrimitiveGizmo.h"
#include "EngineAPI.h"
#include <memory>

class AActor;
class CCamera;
class CPicker;
class UScene;
class USceneComponent;
struct RotationDesc;
struct FMeshData;
class FMaterial;
class FMaterialManager;
struct FRenderCommandQueue;
struct FRay;

enum class EGizmoMode : uint8
{
	Location,
	Rotation,
	Scale
};

enum class EGizmoCoordinateSpace : uint8
{
	World,
	Local
};

enum class EGizmoAxis : uint8
{
	None = 0,
	X,
	Y,
	Z,
	XY,
	XZ,
	YZ,
	XYZ,
	Screen
};

class CGizmo
{
public:
	CGizmo();
	void SetMode(EGizmoMode InMode);
	EGizmoMode GetMode() const { return Mode; }
	void CycleMode();
	void SetCoordinateSpace(EGizmoCoordinateSpace InSpace);
	EGizmoCoordinateSpace GetCoordinateSpace() const { return CoordinateSpace; }
	void ToggleCoordinateSpace();

	void BuildRenderCommands(AActor* SelectedActor, const CCamera* Camera, FRenderCommandQueue& OutQueue) const;
	bool BeginDrag(AActor* SelectedActor, UScene* Scene, const CPicker& Picker, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight);
	bool UpdateDrag(AActor* SelectedActor, UScene* Scene, const CPicker& Picker, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight);
	void UpdateHover(AActor* SelectedActor, UScene* Scene, const CPicker& Picker, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight);
	void ClearHover();
	void EndDrag();

	bool IsDragging() const { return ActiveAxis != EGizmoAxis::None; }

private:
	bool EnsureTranslationMeshes() const;
	bool EnsureRotationMeshes(const CCamera* Camera, const FVector& GizmoWorldLocation) const;
	bool EnsureScaleMeshes() const;
	EGizmoAxis HitTestAxis(AActor* SelectedActor, UScene* Scene, const CPicker& Picker, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight) const;
	bool BeginAxisDrag(EGizmoAxis Axis, AActor* SelectedActor, UScene* Scene, const CPicker& Picker, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight);
	bool BeginTranslationDrag(EGizmoAxis Axis, AActor* SelectedActor, UScene* Scene, const CPicker& Picker, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight);
	bool BeginRotationDrag(EGizmoAxis Axis, AActor* SelectedActor, UScene* Scene, const CPicker& Picker, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight);
	bool BeginScaleDrag(EGizmoAxis Axis, AActor* SelectedActor, UScene* Scene, const CPicker& Picker, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight);
	EGizmoAxis GetDisplayAxis() const;
	RotationDesc BuildRotationDesc(const CCamera* Camera, const FVector& GizmoWorldLocation) const;
	FQuat GetGizmoRotation(const AActor* Actor) const;
	FVector GetGizmoAxisVector(EGizmoAxis Axis, const AActor* Actor) const;
	FVector GetGizmoPlaneNormal(EGizmoAxis Axis, const AActor* Actor) const;

	static FVector GetAxisVector(EGizmoAxis Axis);
	static FVector GetPlaneNormal(EGizmoAxis Axis);
	static FVector GetActorWorldLocation(const AActor* Actor);
	static FQuat GetActorWorldRotation(const AActor* Actor);
	static FQuat GetComponentWorldRotationIgnoringScale(const USceneComponent* Component);
	static FVector GetActorRelativeScale(const AActor* Actor);
	static bool ApplyActorWorldLocation(AActor* Actor, const FVector& NewWorldLocation);
	static bool ApplyActorWorldRotation(AActor* Actor, const FQuat& NewWorldRotation);
	static bool ApplyActorRelativeScale(AActor* Actor, const FVector& NewRelativeScale);
	static bool RayTriangleIntersectTwoSided(const FRay& Ray, const FVector& V0, const FVector& V1, const FVector& V2, float& OutDistance);
	static bool IntersectPlane(const FRay& Ray, const FVector& PlaneOrigin, const FVector& PlaneNormal, FVector& OutIntersection);

	float ComputeGizmoScale(const FVector& WorldPosition, const CCamera* Camera) const;
	float GetRenderGizmoScale(float BaseGizmoScale) const;

private:
	EGizmoMode Mode = EGizmoMode::Location;
	EGizmoCoordinateSpace CoordinateSpace = EGizmoCoordinateSpace::World;
	EGizmoAxis ActiveAxis = EGizmoAxis::None;
	EGizmoAxis HoveredAxis = EGizmoAxis::None;
	float CurrentRotationDeltaDegrees = 0.0f;
	FVector DragStartActorLocation = FVector::ZeroVector;
	FVector DragStartGizmoLocation = FVector::ZeroVector;
	FVector DragStartIntersection = FVector::ZeroVector;
	FVector DragPlaneNormal = FVector::ZeroVector;
	FVector DragStartRotationVector = FVector::ZeroVector;
	FVector DragStartActorScale = FVector::OneVector;
	float DragStartAxisDistance = 0.0f;
	FQuat DragStartActorRotation = FQuat::Identity;
	int32 DragStartScreenX = 0;
	int32 DragStartScreenY = 0;

	std::shared_ptr<FMaterial> Material;

	mutable std::unique_ptr<CPrimitiveGizmo> TranslationGizmo;
	mutable std::unique_ptr<CPrimitiveGizmo> ScaleGizmo;
	mutable std::shared_ptr<FMeshData> TranslationAxisMeshes[3];
	mutable std::shared_ptr<FMeshData> TranslationPlaneMeshes[3];
	mutable std::shared_ptr<FMeshData> TranslationScreenMesh;
	mutable std::shared_ptr<FMeshData> RotationAxisMeshes[3];
	mutable std::shared_ptr<FMeshData> RotationScreenMesh;
	mutable std::shared_ptr<FMeshData> ScaleAxisMeshes[3];
	mutable std::shared_ptr<FMeshData> ScalePlaneMeshes[3];
	mutable std::shared_ptr<FMeshData> ScaleCenterMesh;
	mutable std::shared_ptr<FMeshData> HighlightTranslationAxes[3];
	mutable std::shared_ptr<FMeshData> HighlightTranslationPlanes[3];
	mutable std::shared_ptr<FMeshData> HighlightTranslationScreenMesh;
	mutable std::shared_ptr<FMeshData> HighlightRotationAxes[3];
	mutable std::shared_ptr<FMeshData> HighlightRotationScreenMesh;
	mutable std::shared_ptr<FMeshData> HighlightScaleAxes[3];
	mutable std::shared_ptr<FMeshData> HighlightScalePlanes[3];
	mutable std::shared_ptr<FMeshData> HighlightScaleCenterMesh;
	mutable FVector CachedRotationCameraDirection = FVector::ZeroVector;
	mutable FVector CachedRotationViewUp = FVector::ZeroVector;
	mutable FVector CachedRotationViewRight = FVector::ZeroVector;
	mutable bool CachedRotationDragging = false;
	mutable EGizmoAxis CachedRotationActiveAxis = EGizmoAxis::None;
};
