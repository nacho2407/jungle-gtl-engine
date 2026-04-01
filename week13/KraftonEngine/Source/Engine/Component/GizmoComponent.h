#pragma once

#include "PrimitiveComponent.h"
#include "Core/CoreTypes.h"
#include "Render/Types/ViewTypes.h"

class AActor;

enum EGizmoMode
{
	Translate,
	Rotate,
	Scale,
	End
};

class UGizmoComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UGizmoComponent, UPrimitiveComponent)
	UGizmoComponent();

	bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;

	FVector GetVectorForAxis(int32 Axis);
	void RenderGizmo() {}
	void SetTarget(AActor* NewTarget);
	void SetSelectedActors(const TArray<AActor*>* InSelectedActors) { AllSelectedActors = InSelectedActors; }
	inline void SetHolding(bool bHold) { bIsHolding = bHold; }
	inline bool IsHolding() const { return bIsHolding; }
	inline bool IsHovered() const { return SelectedAxis != -1; }
	inline bool HasTarget() const { return TargetActor != nullptr; }
	inline AActor* GetTarget() const { return TargetActor; }
	inline int32 GetSelectedAxis() const { return SelectedAxis; }

	inline void SetPressedOnHandle(bool bPressed) { bPressedOnHandle = bPressed; }
	inline bool IsPressedOnHandle() const { return bPressedOnHandle; }

	void SetAxisMask(uint32 InMask) { AxisMask = InMask; }
	uint32 GetAxisMask() const { return AxisMask; }
	EGizmoMode GetMode() const { return CurMode; }
	void UpdateAxisMask(ELevelViewportType ViewportType);
	void UpdateHoveredAxis(int Index);
	void UpdateDrag(const FRay& Ray);
	void DragEnd();

	void SetTargetLocation(FVector NewLocation);
	void SetTargetRotation(FVector NewRotation);
	void SetTargetScale(FVector NewScale);


	void SetNextMode();
	void UpdateGizmoMode(EGizmoMode NewMode);
	inline void SetTranslateMode() { UpdateGizmoMode(EGizmoMode::Translate); }
	inline void SetRotateMode() { UpdateGizmoMode(EGizmoMode::Rotate); }
	inline void SetScaleMode() { UpdateGizmoMode(EGizmoMode::Scale); }
	void UpdateGizmoTransform();
	float ComputeScreenSpaceScale(const FVector& CameraLocation, bool bIsOrtho = false, float OrthoWidth = 10.0f);
	void ApplyScreenSpaceScaling(const FVector& CameraLocation, bool bIsOrtho = false, float OrthoWidth = 10.0f);
	void SetWorldSpace(bool bWorldSpace);


	//UActorComponent Override
	void Deactivate() override;

	FMeshBuffer* GetMeshBuffer() const override;
	void CollectRender(FRenderBus& Bus) const override;
	void CollectSelection(FRenderBus& Bus) const override {}  // Gizmo는 선택 이펙트 없음

private:
	bool IntersectRayAxis(const FRay& Ray, FVector AxisEnd, float& OutRayT);

	//Control Target Method
	void HandleDrag(float DragAmount);
	void TranslateTarget(float DragAmount);
	void RotateTarget(float DragAmount);
	void ScaleTarget(float DragAmount);

	void UpdateLinearDrag(const FRay& Ray);
	void UpdateAngularDrag(const FRay& Ray);

private:
	AActor* TargetActor = nullptr;
	const TArray<AActor*>* AllSelectedActors = nullptr;
	EGizmoMode CurMode = EGizmoMode::Translate;
	FVector LastIntersectionLocation;
	const float AxisLength = 1.0f;
	float Radius = 0.1f;
	const float ScaleSensitivity = 1.0f;
	static constexpr float GizmoScreenScale = 0.15f; // 화면 대비 기즈모 크기 비율
	int32 SelectedAxis = -1;
	bool bIsFirstFrameOfDrag = true;
	bool bIsHolding = false;
	bool bIsWorldSpace = true;
	bool bPressedOnHandle = false;
	const FMeshData* MeshData = nullptr;
	uint32 AxisMask = 0x7; // 비트 0=X, 1=Y, 2=Z — 기본 전부 표시
};