#pragma once

#include "PrimitiveComponent.h"

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

	USceneComponent* TargetComponent = nullptr;
	EGizmoMode CurMode = EGizmoMode::Translate;
	FVector LastIntersectionLocation;
	const float axisLength = 1.0f;
	float Radius = 0.1f;
	const float ScaleSensitivity = 1.0f;
	int32 SelectedAxis = -1;
	bool bIsFirstFrameOfDrag = true;
	bool bIsHolding = false;
	bool bIsWorldSpace = true;
	bool bPressedOnHandle = false;

	bool IntersectRayAxis(const FRay& Ray, FVector AxisEnd, float& OutRayT);

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

	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;

	FVector GetVectorForAxis(int32 axis);
	void RenderGizmo() {}
	void SetTarget(USceneComponent* NewTargetComponent);
	inline void SetHolding(bool bHold) { bIsHolding = bHold; }
	inline bool IsHolding() { return bIsHolding; }
	inline bool IsHovered() const { return SelectedAxis != -1; }
	inline bool HasTarget() const { return TargetComponent != nullptr; }
	inline USceneComponent* GetTarget() const { return TargetComponent; }
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
	void SetWorldSpace(bool bWorldSpace);

	bool GetRenderCommand(FRenderCommand& OutCommand) override;

	//UActorComponent Override
	void Deactivate() override;

	EPrimitiveType GetPrimitiveType() const override;
};