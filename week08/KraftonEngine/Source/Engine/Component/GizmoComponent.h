#pragma once

#include "PrimitiveComponent.h"
#include "Core/CoreTypes.h"
#include "Math/Rotator.h"
#include "Render/Types/ViewTypes.h"

class AActor;
class FPrimitiveSceneProxy;
class FScene;

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

	FVector GetVectorForAxis(int32 Axis) const;
	void RenderGizmo() {}
	void SetTarget(AActor* NewTarget);
	void SetSelectedActors(const TArray<AActor*>* InSelectedActors) { AllSelectedActors = InSelectedActors; }
	void SetHolding(bool bHold);
	inline bool IsHolding() const { return bIsHolding; }
	inline bool IsHovered() const { return SelectedAxis != -1; }
	inline bool HasTarget() const { return TargetActor != nullptr; }
	inline AActor* GetTarget() const { return TargetActor; }
	inline int32 GetSelectedAxis() const { return SelectedAxis; }

	inline void SetPressedOnHandle(bool bPressed) { bPressedOnHandle = bPressed; }
	inline bool IsPressedOnHandle() const { return bPressedOnHandle; }

	EGizmoMode GetMode() const { return CurMode; }
	void SetAxisMask(uint32 InMask) { AxisMask = InMask; }
	uint32 GetAxisMask() const { return AxisMask; }

	// ViewportType + GizmoMode → AxisMask 계산 (Proxy에서도 사용)
	static uint32 ComputeAxisMask(ELevelViewportType ViewportType, EGizmoMode Mode);
	void UpdateHoveredAxis(int Index);
	void UpdateDrag(const FRay& Ray);
	void DragEnd();

	void SetTargetLocation(FVector NewLocation);
	void SetTargetRotation(FRotator NewRotation);
	void SetTargetScale(FVector NewScale);


	void SetNextMode();
	void UpdateGizmoMode(EGizmoMode NewMode);
	inline void SetTranslateMode() { UpdateGizmoMode(EGizmoMode::Translate); }
	inline void SetRotateMode() { UpdateGizmoMode(EGizmoMode::Rotate); }
	inline void SetScaleMode() { UpdateGizmoMode(EGizmoMode::Scale); }
	void UpdateGizmoTransform();
	float ComputeScreenSpaceScale(const FVector& CameraLocation, bool bIsOrtho = false, float OrthoWidth = 10.0f) const;
	void ApplyScreenSpaceScaling(const FVector& CameraLocation, bool bIsOrtho = false, float OrthoWidth = 10.0f);
	void SetWorldSpace(bool bWorldSpace);

	void MarkGizmoDirty(EDirtyFlag Flag);
	void MarkAllGizmoDirty();

	//UActorComponent Override
	void Deactivate() override;

	FMeshBuffer* GetMeshBuffer() const override;
	FMeshDataView GetMeshDataView() const override { return MeshData ? FMeshDataView::FromMeshData(*MeshData) : FMeshDataView{}; }
	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void CreateRenderState() override;
	void DestroyRenderState() override;

	// Actor 없이 독립 생성된 Gizmo용 — 외부에서 Scene을 직접 지정
	void SetScene(FScene* InScene) { RegisteredScene = InScene; }

private:
	bool IntersectRayAxis(const FRay& Ray, FVector AxisEnd, float AxisScale, float& OutRayT);
	bool IntersectRayRotationHandle(const FRay& Ray, int32 Axis, float& OutRayT) const;

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
	uint32 AxisMask = 0x7; // 비트 0=X, 1=Y, 2=Z — LineTrace용 (렌더링은 Proxy가 직접 계산)
	FPrimitiveSceneProxy* InnerProxy = nullptr;	// GizmoInner 전용 프록시
	FScene* RegisteredScene = nullptr;			// Actor 없이 독립 생성 시 사용
};
