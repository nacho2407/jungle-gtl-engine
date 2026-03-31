#pragma once

#include "PrimitiveComponent.h"
#include "Core/CoreMinimal.h"

class AActor;
struct FMeshData;

class UGizmoComponent : public UPrimitiveComponent
{

private:
	enum EGizmoMode
	{
		Translate,
		Rotate,
		Scale,
		End
	};

	AActor* TargetActor = nullptr;
	const TArray<AActor*>* AllSelectedActors = nullptr;
	EGizmoMode CurMode = EGizmoMode::Translate;
	FVector LastIntersectionLocation;
	const float AxisLength = 1.0f;
	float Radius = 0.1f;
	const float ScaleSensitivity = 1.0f;
	int32 SelectedAxis = -1;
	bool bIsFirstFrameOfDrag = true;
	bool bIsHolding = false;
	bool bIsWorldSpace = true;
	bool bPressedOnHandle = false;

	bool IntersectRayAxis(const FRay& Ray, FVector AxisEnd, float& OutRayT);
	const FMeshData* GetActiveMeshData() const;

	//Control Target Method
	void HandleDrag(float DragAmount);
	void TranslateTarget(float DragAmount);
	void RotateTarget(float DragAmount);
	void ScaleTarget(float DragAmount);

	void UpdateLinearDrag(const FRay& Ray);
	void UpdateAngularDrag(const FRay& Ray);

public:
	DECLARE_CLASS(UGizmoComponent, UPrimitiveComponent)
	UGizmoComponent();

	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;

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
	void ApplyScreenSpaceScaling(const FVector& CameraLocation);
	// 직교 뷰 전용: OrthoHeight 기반으로 기즈모 스케일 설정
	void ApplyScreenSpaceScalingOrtho(float OrthoHeight);
	void SetWorldSpace(bool bWorldSpace);
	bool IsWorldSpace() const { return bIsWorldSpace; }


	//UActorComponent Override
	void Deactivate() override;

	EPrimitiveType GetPrimitiveType() const override;

private:
	const FMeshData* GizmoMeshData = nullptr;
	FVector LocalExtents = FVector(1.5f, 1.5f, 1.5f);
};
