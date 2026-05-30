#pragma once

#include "GizmoTransformTarget.h"
#include "Object/Ptr/WeakObjectPtr.h"

class UPhysicsAssetDebugComponent;
struct FConstraintInstanceInitDesc;

class FPhysicsAssetConstraintGizmoTarget : public IGizmoTransformTarget
{
public:
	FPhysicsAssetConstraintGizmoTarget() = default;
	FPhysicsAssetConstraintGizmoTarget(UPhysicsAssetDebugComponent* InDebugComponent, int32 InConstraintIndex);
	~FPhysicsAssetConstraintGizmoTarget() override = default;

	void SetConstraint(UPhysicsAssetDebugComponent* InDebugComponent, int32 InConstraintIndex);
	void Clear();

	bool IsValid() const override;
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

	int32 GetConstraintIndex() const { return ConstraintIndex; }

private:
	UPhysicsAssetDebugComponent* GetDebugComponent() const;
	FConstraintInstanceInitDesc* GetConstraintDesc() const;

private:
	TWeakObjectPtr<UPhysicsAssetDebugComponent> DebugComponent;
	int32 ConstraintIndex = -1;
};
