#pragma once

#include "Component/PrimitiveComponent.h"
#include "Core/Types/CoreTypes.h"
#include "Math/Rotator.h"
#include "Render/Types/ViewTypes.h"
#include "Gizmo/GizmoTransformTarget.h"
#include "Gizmo/ComponentGizmoTarget.h"

#include "Source/Engine/Component/Debug/GizmoComponent.generated.h"
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

UCLASS()
class UGizmoComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY()
	UGizmoComponent();

	bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;

	FVector GetVectorForAxis(int32 Axis) const;
	FVector GetLocalAxisVector(int32 Axis) const;

	void RenderGizmo() {}
	void SetSelectedActors(const TArray<AActor*>* InSelectedActors) { AllSelectedActors = InSelectedActors; }
	void SetHolding(bool bHold);
	inline bool IsHolding() const { return bIsHolding; }
	inline bool IsHovered() const { return SelectedAxis != -1; }

	inline bool HasTarget() const { return Target != nullptr; }
	inline IGizmoTransformTarget* GetTarget() const { return Target; }
	inline USceneComponent* GetTargetComponent() const { return ComponentTarget.IsValid() ? ComponentTarget.GetComponent() : nullptr; }

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

	void SetNextMode();
	void UpdateGizmoMode(EGizmoMode NewMode);
	inline void SetTranslateMode() { UpdateGizmoMode(EGizmoMode::Translate); }
	inline void SetRotateMode() { UpdateGizmoMode(EGizmoMode::Rotate); }
	inline void SetScaleMode() { UpdateGizmoMode(EGizmoMode::Scale); }
	void UpdateGizmoTransform();
	float ComputeScreenSpaceScale(const FVector& CameraLocation, bool bIsOrtho = false, float OrthoWidth = 10.0f) const;
	void ApplyScreenSpaceScaling(const FVector& CameraLocation, bool bIsOrtho = false, float OrthoWidth = 10.0f);
	void SetWorldSpace(bool bWorldSpace);
	bool IsWorldSpace() const { return bIsWorldSpace; }
	void SetSceneRenderingEnabled(bool bEnabled);
	bool IsSceneRenderingEnabled() const { return bSceneRenderingEnabled; }
	void SetSnapSettings(bool bTranslationEnabled, float InTranslationSnapSize,
		bool bRotationEnabled, float InRotationSnapSizeDegrees,
		bool bScaleEnabled, float InScaleSnapSize);


	//UActorComponent Override
	void Deactivate() override;

	FMeshBuffer* GetMeshBuffer() const override;
	FMeshDataView GetMeshDataView() const override { return MeshData ? FMeshDataView::FromMeshData(*MeshData) : FMeshDataView{}; }
	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void CreateRenderState() override;
	void DestroyRenderState() override;

	// Actor 없이 독립 생성된 Gizmo용 — 외부에서 Scene을 직접 지정
	void SetScene(FScene* InScene) { RegisteredScene = InScene; }

	// Target 지원
	void SetTarget(IGizmoTransformTarget* NewTarget);
	void SetTarget(USceneComponent* NewTarget);
	void SetTarget(AActor* NewTarget);

private:
	bool IntersectRayAxis(const FRay& Ray, FVector AxisEnd, float AxisScale, float& OutRayT);
	bool IntersectRayRotationHandle(const FRay& Ray, int32 Axis, float& OutRayT) const;

	//Control Target Method
	void HandleDrag(float DragAmount);
	float ApplySnapToDragAmount(float DragAmount);
	void ResetSnapAccumulation();
	bool HasMultipleSelectedActorTargets() const;
	FVector GetTargetPivotLocation() const;
	bool TranslateSelectedActorTargets(const FVector& Delta);
	bool RotateSelectedActorTargets(const FQuat& DeltaQuat);
	bool ScaleSelectedActorTargets(const FVector& Delta);
	void TranslateTarget(float DragAmount);
	void RotateTarget(float DragAmount);
	void ScaleTarget(float DragAmount);

	void UpdateLinearDrag(const FRay& Ray);
	void UpdateAngularDrag(const FRay& Ray);

	/**
	* @brief 회전 드래그 기준 평면 상태를 초기화합니다.
	*/
	void ResetAngularDragBasis();

	/**
	* @brief 고정된 회전 드래그 평면 위의 현재 마우스 방향 계산
	*
	* @param Ray 현재 마우스 위치에서 디프로젝트한 world ray
	*
	* @param OutDirection 회전 pivot에서 현재 교차점으로 향하는 정규화 방향 벡터
	*
	* @return 유효한 교차 방향 계산 여부
	*/
	bool TryGetAngularDragDirection(const FRay& Ray, FVector& OutDirection) const;

private:
	IGizmoTransformTarget* Target = nullptr;
	FComponentGizmoTarget ComponentTarget;

	const TArray<AActor*>* AllSelectedActors = nullptr;
	EGizmoMode CurMode = EGizmoMode::Translate;
	FVector LastIntersectionLocation;
	FVector AngularDragPivotLocation = FVector::ZeroVector;
	FVector AngularDragAxisVector = FVector::ZeroVector;
	FVector LastAngularDragDirection = FVector::ZeroVector;
	const float AxisLength = 1.0f;
	float Radius = 0.1f;
	const float ScaleSensitivity = 1.0f;
	static constexpr float GizmoScreenScale = 0.15f; // 화면 대비 기즈모 크기 비율
	int32 SelectedAxis = -1;
	bool bIsFirstFrameOfDrag = true;
	bool bAngularDragBasisValid = false;
	bool bIsHolding = false;
	bool bIsWorldSpace = true;
	bool bSceneRenderingEnabled = true;
	bool bPressedOnHandle = false;
	bool bTranslationSnapEnabled = false;
	bool bRotationSnapEnabled = false;
	bool bScaleSnapEnabled = false;
	float TranslationSnapSize = 10.0f;
	float RotationSnapSizeRadians = 0.261799395f;
	float ScaleSnapSize = 0.1f;
	float AccumulatedRawDragAmount = 0.0f;
	float LastAppliedSnappedDragAmount = 0.0f;
	const FMeshData* MeshData = nullptr;
	uint32 AxisMask = 0x7; // 비트 0=X, 1=Y, 2=Z — LineTrace용 (렌더링은 Proxy가 직접 계산)
	FPrimitiveSceneProxy* InnerProxy = nullptr;	// GizmoInner 전용 프록시
	FScene* RegisteredScene = nullptr;			// Actor 없이 독립 생성 시 사용
};
