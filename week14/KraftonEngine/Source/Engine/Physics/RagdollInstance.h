#pragma once

#include "Core/Types/CoreTypes.h"
#include "Physics/BodyInstance.h"

class FPhysicsScene;
class USkeletalMeshComponent;
class UPhysicsAsset;
struct FConstraintInstance;

struct FRagdollInstance
{
	TArray<FBodyInstance> Bodies;
	TArray<int32> BodyToBoneIndex;
	TArray<FConstraintInstance*> Constraints;
	TArray<FTransform> InitialLocalPose;
	FVector ComponentWorldScaleAtStart = FVector::OneVector;
	int32 AnchorBodyIndex = -1;

	bool bInitialized = false;
	bool IsActive() const {return bInitialized;}

	void Initialize(UPhysicsAsset* Asset, USkeletalMeshComponent* MeshComp, FPhysicsScene* Scene, const FVector& InitialLinearVelocity);
	void Release(FPhysicsScene* Scene);

	bool GetAnchorWorldLocation(FVector& OutWorldLocation) const;
	bool BuildLocalPoseFromBodies(USkeletalMeshComponent* MeshComp, TArray<FTransform>& OutLocalPose) const;
	void SyncBonesFromBodies(USkeletalMeshComponent* MeshComp); // 시뮬레이션 결과(바디 월드)를 본 포즈로 역기입. 매 틱 호출
};
