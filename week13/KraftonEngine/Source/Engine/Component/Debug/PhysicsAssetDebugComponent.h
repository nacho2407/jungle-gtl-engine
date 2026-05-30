#pragma once

#include "Component/PrimitiveComponent.h"
#include "Object/Ptr/WeakObjectPtr.h"
#include "PhysicsEngine/ShapeElem.h"

#include "Source/Engine/Component/Debug/PhysicsAssetDebugComponent.generated.h"

class UPhysicsAsset;
class USkeletalMeshComponent;

enum class EPhysicsAssetDebugHitType : uint8
{
	None,
	Body,
	Constraint
};

struct FPhysicsAssetDebugHitResult
{
	EPhysicsAssetDebugHitType Type = EPhysicsAssetDebugHitType::None;
	EAggCollisionShape ShapeType = EAggCollisionShape::Unknown;
	int32 BodyIndex = -1;
	int32 ConstraintIndex = -1;
	int32 ShapeIndex = -1;
	float Distance = 3.402823466e+38F;
	FVector WorldHitLocation = FVector::ZeroVector;
	FVector WorldNormal = FVector::ZeroVector;
};

UCLASS()
class UPhysicsAssetDebugComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY()

	UPhysicsAssetDebugComponent();
	~UPhysicsAssetDebugComponent() override;

	FPrimitiveSceneProxy* CreateSceneProxy() override;
	bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;
	bool SupportsOutline() const override { return false; }

	USkeletalMeshComponent* GetTargetSkeletalMeshComponent() const { return TargetSkeletalMeshComponent.Get(); }
	UPhysicsAsset* GetPhysicsAsset() const { return PhysicsAsset.Get(); }

	void SetTarget(USkeletalMeshComponent* InTargetSkeletalMeshComponent, UPhysicsAsset* InPhysicsAsset);

	bool PickBody(const FRay& Ray, FPhysicsAssetDebugHitResult& OutHit) const;
	bool GetPhysicsAssetBoneWorldTransform(const FName& BoneName, FTransform& OutBoneWorldTM) const;

	int32 GetSelectedBodyIndex() const { return SelectedBodyIndex; }
	void SetSelectedBodyIndex(int32 InSelectedBodyIndex);

	int32 GetSelectedConstraintIndex() const { return SelectedConstraintIndex; }
	void SetSelectedConstraintIndex(int32 InSelectedConstraintIndex);

	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	TWeakObjectPtr<USkeletalMeshComponent> TargetSkeletalMeshComponent;
	TWeakObjectPtr<UPhysicsAsset> PhysicsAsset;
	int32 SelectedBodyIndex = -1;
	int32 SelectedConstraintIndex = -1;
};
