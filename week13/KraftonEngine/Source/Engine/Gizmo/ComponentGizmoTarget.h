#pragma once
#include "GizmoTransformTarget.h"
#include "Object/Ptr/WeakObjectPtr.h"

class USceneComponent;

class FComponentGizmoTarget : public IGizmoTransformTarget
{
public:
	FComponentGizmoTarget();
	FComponentGizmoTarget(USceneComponent* InComponent);
	~FComponentGizmoTarget() override = default;

public:
	USceneComponent* GetComponent() const;
	void SetComponent(USceneComponent* InComponent);

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

private:
	TWeakObjectPtr<USceneComponent> Component;
};
