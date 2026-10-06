#pragma once

#include "GizmoTransformTarget.h"
#include "Math/Transform.h"
#include "Object/Ptr/WeakObjectPtr.h"

class UBodySetup;
class UPhysicsAssetDebugComponent;

enum class EAggCollisionShape : uint8;

class FPhysicsAssetShapeGizmoTarget : public IGizmoTransformTarget
{
public:
	FPhysicsAssetShapeGizmoTarget() = default;
	FPhysicsAssetShapeGizmoTarget(UPhysicsAssetDebugComponent* InDebugComponent, int32 InBodyIndex);
	~FPhysicsAssetShapeGizmoTarget() override = default;

	void SetShape(UPhysicsAssetDebugComponent* InDebugComponent, int32 InBodyIndex);
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

	int32 GetBodyIndex() const { return BodyIndex; }
	EAggCollisionShape GetShapeType() const;

private:
	struct FEditableShape;

	UPhysicsAssetDebugComponent* GetDebugComponent() const;
	UBodySetup* GetBodySetup() const;
	bool GetBoneEditTransform(FTransform& OutBoneTM, float& OutUniformScale) const;
	bool GetEditableShape(FEditableShape& OutShape) const;
	FTransform GetShapeWorldTransform() const;
	void MarkShapeChanged() const;

private:
	TWeakObjectPtr<UPhysicsAssetDebugComponent> DebugComponent;
	int32 BodyIndex = -1;
};
