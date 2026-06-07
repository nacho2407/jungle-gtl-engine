#pragma once
#include "Component/MeshComponent.h"

#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Object/Ptr/SoftObjectPtr.h"

#include "Source/Engine/Component/Primitive/SkinnedMeshComponent.generated.h"
struct FSkeletalMesh;
class USkeletalMesh;
class UMaterialInterface;
class UPhysicsAsset;

// ==================================================================================
// SkeletalMesh의 런타임 상태를 소유하는 기본 컴포넌트.
// Mesh/Material 경로 관리, CPU skinning 결과, bone edit pose, bounds dirty 처리를
// 한 곳에 모아 USkeletalMeshComponent가 렌더 proxy용 얇은 wrapper로 남을 수 있게 한다.
// ==================================================================================
UCLASS()
class USkinnedMeshComponent : public UMeshComponent
{
public:
	GENERATED_BODY()
	USkinnedMeshComponent() = default;
	~USkinnedMeshComponent() override = default;

	// Mesh assignment 섹션: SkeletalMesh 교체 시 필요한 캐시와 dirty 처리를 한 번의 흐름으로 끝낸다.
	virtual void SetSkeletalMesh(USkeletalMesh* InMesh);
	USkeletalMesh* GetSkeletalMesh() const;

	// PhysicsAsset 접근: per-instance 오버라이드 우선, 없으면 메시 기본값(USkeletalMesh).
	void SetPhysicsAssetOverride(UPhysicsAsset* InAsset) { PhysicsAssetOverride = InAsset; }
	UPhysicsAsset* GetPhysicsAsset();

	// Bounds 섹션: SkeletalMesh는 local asset bounds 대신 실제 skinned vertex 기준으로 culling bounds를 만든다.
	void UpdateWorldAABB() const override;

	// Material 섹션: editor slot 경로와 runtime override 포인터를 같이 유지한다.
	void SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial);
	UMaterialInterface* GetMaterial(int32 ElementIndex) const;
	const TArray<UMaterialInterface*>& GetOverrideMaterials() const { return OverrideMaterials; }

	// Serialization/editor 섹션: asset pointer는 저장하지 않고 path를 저장한 뒤 로드 후 SetSkeletalMesh 흐름으로 복원한다.
	void PostDuplicate() override;

	void PostEditProperty(const char* PropertyName) override;
	bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;

	const FString& GetSkeletalMeshPath() const { return SkeletalMeshPath.ToString(); }

	// Bone edit 섹션: bone getter/setter는 edit pose를 만들고 CPU skinning/cache revision까지 갱신해야 한다.
	void EnsureBoneEditPose();
	void ResetBoneEditPose();

	UFUNCTION(Lua)
	int32 FindBoneIndex(const FString& BoneName) const;
	UFUNCTION(Lua)
	bool GetBoneWorldTransformByIndex(int32 BoneIndex, FTransform& OutTransform) const;
	UFUNCTION(Lua)
	bool GetBoneWorldTransformByName(const FString& BoneName, FTransform& OutTransform) const;

	// 디버그 드로우/물리용: TRS 분해 없이 본 월드 행렬을 그대로 반환(스케일/시어 보존).
	bool GetBoneWorldMatrixByIndex(int32 BoneIndex, FMatrix& OutMatrix) const;
	bool GetBoneWorldMatrixByName(const FString& BoneName, FMatrix& OutMatrix) const;
	bool GetReferenceBoneGlobalMatrixByName(const FString& BoneName, FMatrix& OutMatrix) const;

	UFUNCTION(Lua)
	bool GetBoneSocketWorldTransform(const FString& BoneName, const FTransform& LocalOffset, FTransform& OutTransform) const;

	FVector GetBoneLocationByIndex(int32 BoneIndex) const;
	FRotator GetBoneRotationByIndex(int32 BoneIndex) const;
	FQuat GetBoneQuatByIndex(int32 BoneIndex) const;
	FVector GetBoneScaleByIndex(int32 BoneIndex) const;
	FTransform GetBoneLocalTransformByIndex(int32 BoneIndex) const;
	FTransform GetBoneEditBaseLocalTransformByIndex(int32 BoneIndex) const;

	void SetBoneLocationByIndex(int32 BoneIndex, const FVector& NewLocation);
	void SetBoneRotationByIndex(int32 BoneIndex, const FRotator& NewRotation);
	void SetBoneRotationByIndex(int32 BoneIndex, const FQuat& NewQuat);
	void SetBoneScaleByIndex(int32 BoneIndex, const FVector& NewScale);
	void SetBoneLocalTransformByIndex(int32 BoneIndex, const FTransform& NewLocalTransform);
	void SetBoneEditBaseLocalTransformByIndex(int32 BoneIndex, const FTransform& NewLocalTransform);

	void SetBoneLocalTransforms(const TArray<FTransform>& LocalPose);
	void SetAnimationPose(const TArray<FTransform>& LocalPose, const TArray<float>& InMorphTargetWeights);
	void ApplyBoneEditBasePose();

	int32 FindMorphTargetIndex(const FString& TargetName) const;
	void  SetMorphTargetWeight(const FString& TargetName, float Weight);
	void  SetMorphTargetWeightByIndex(int32 TargetIndex, float Weight);
	void  SetMorphTargetWeights(const TArray<float>& Weights);
	void  ClearMorphTargetWeights();
	float GetMorphTargetWeight(const FString& TargetName) const;
	float GetMorphTargetWeightByIndex(int32 TargetIndex) const;
	bool  HasActiveMorphTargets() const;

	const TArray<float>& GetMorphTargetWeights() const
	{
		return MorphTargetWeights;
	}

	void GetCurrentBoneGlobalTransforms(TArray<FTransform>& OutGlobals) const;
	void GetCurrentBoneGlobalMatrices(TArray<FMatrix>& OutGlobals) const;
	void BuildSkinMatrices(TArray<FMatrix>& OutSkinMatrices) const;
	const TArray<FVertexPNCTT>& GetSkinnedVertices() const { return SkinnedVertices; }
	uint64 GetSkinnedRevision() const { return SkinnedRevision; }
	FMeshBuffer* GetMeshBuffer() const override;
	FMeshDataView GetMeshDataView() const override;

protected:
	// Tick/skinning 섹션: animation system 없이 현재 bone edit pose를 매 frame CPU skinning 결과로 반영한다.
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	void InitSkinningCache();
	void UpdateCPUSkinning();
	void RefreshSkinningAfterPoseChanged();
	void RefreshSkinningAfterMorphChanged();
	void InitMorphTargetWeights();
	void ApplyMorphTargetWeightsNoRefresh(const TArray<float>& Weights);
	void BuildMorphedVertexData(
		const FSkeletalMesh& Asset,
		TArray<FVector>&     OutPositions,
		TArray<FVector>&     OutNormals
		) const;
	void EnsureBoneEditBasePose();
	void BuildBoneEditGlobalTransforms(TArray<FTransform>& OutGlobals) const;
	void BuildBoneEditGlobalMatrices(TArray<FMatrix>& OutGlobals) const;

protected:
	// Mesh/material state는 SetSkeletalMesh와 PostEditProperty가 같은 경로를 쓰도록 여기서 소유한다.
	TObjectPtr<USkeletalMesh> SkeletalMesh;
	UPROPERTY(Edit, Save, Category="Mesh", DisplayName="Skeletal Mesh", AssetType="SkeletalMesh")
	FSoftObjectPtr SkeletalMeshPath = "None";
	TArray<UMaterialInterface*> OverrideMaterials;
	UPROPERTY(Edit, Save, EditFixedSize, Category="Materials", DisplayName="Materials", AssetType="Material")
	TArray<FSoftObjectPtr> MaterialSlots;

	// PhysicsAsset 오버라이드 (per-instance). nullptr면 메시 기본값 사용. 런타임 전용(직렬화는 나중).
	UPhysicsAsset* PhysicsAssetOverride = nullptr;

	// BoneEditLocalMatrices is the current evaluated pose. BoneEditBaseLocalMatrices is the
	// edited animation base pose that survives animation evaluation and is not used as skin bind.
	TArray<FMatrix> BoneEditLocalMatrices;
	bool bUseBoneEditPose = false;
	TArray<FMatrix> BoneEditBaseLocalMatrices;
	bool bUseBoneEditBasePose = false;

	// Component-local morph runtime state.
	// 메인 Actor/Component Details에는 노출하지 않는다.
	// Mesh Editor preview, Animation curve, Lua override가 public API를 통해서만 제어한다.
	TArray<float> MorphTargetWeights;

	// SceneProxy는 이 결과와 revision만 보고 dynamic vertex buffer를 갱신한다.
	TArray<FVertexPNCTT> SkinnedVertices;
	uint64 SkinnedRevision = 0;
};
