#pragma once
#include "MeshComponent.h"

#include "Math/Rotator.h"
#include "Math/Transform.h"

class USkeletalMesh;
class UMaterial;

// ==================================================================================
// SkeletalMesh의 런타임 상태를 소유하는 기본 컴포넌트.
// Mesh/Material 경로 관리, CPU skinning 결과, bone edit pose, bounds dirty 처리를
// 한 곳에 모아 USkeletalMeshComponent가 렌더 proxy용 얇은 wrapper로 남을 수 있게 한다.
// ==================================================================================
class USkinnedMeshComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)

	USkinnedMeshComponent() = default;
	~USkinnedMeshComponent() override = default;

	// Mesh assignment 섹션: SkeletalMesh 교체 시 필요한 캐시와 dirty 처리를 한 번의 흐름으로 끝낸다.
	virtual void SetSkeletalMesh(USkeletalMesh* InMesh);
	USkeletalMesh* GetSkeletalMesh() const;

	// Bounds 섹션: SkeletalMesh는 local asset bounds 대신 실제 skinned vertex 기준으로 culling bounds를 만든다.
	void UpdateWorldAABB() const override;

	// Material 섹션: editor slot 경로와 runtime override 포인터를 같이 유지한다.
	void SetMaterial(int32 ElementIndex, UMaterial* InMaterial);
	UMaterial* GetMaterial(int32 ElementIndex) const;
	const TArray<UMaterial*>& GetOverrideMaterials() const { return OverrideMaterials; }

	// Serialization/editor 섹션: asset pointer는 저장하지 않고 path를 저장한 뒤 로드 후 SetSkeletalMesh 흐름으로 복원한다.
	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult) override;

	const FString& GetSkeletalMeshPath() const { return SkeletalMeshPath; }

	// Bone edit 섹션: bone getter/setter는 edit pose를 만들고 CPU skinning/cache revision까지 갱신해야 한다.
	void EnsureBoneEditPose();
	void ResetBoneEditPose();

	FVector GetBoneLocationByIndex(int32 BoneIndex) const;
	FRotator GetBoneRotationByIndex(int32 BoneIndex) const;
	FQuat GetBoneQuatByIndex(int32 BoneIndex) const;
	FVector GetBoneScaleByIndex(int32 BoneIndex) const;
	FTransform GetBoneLocalTransformByIndex(int32 BoneIndex) const;

	void SetBoneLocationByIndex(int32 BoneIndex, const FVector& NewLocation);
	void SetBoneRotationByIndex(int32 BoneIndex, const FRotator& NewRotation);
	void SetBoneRotationByIndex(int32 BoneIndex, const FQuat& NewQuat);
	void SetBoneScaleByIndex(int32 BoneIndex, const FVector& NewScale);
	void SetBoneLocalTransformByIndex(int32 BoneIndex, const FTransform& NewLocalTransform);

	void GetCurrentBoneGlobalTransforms(TArray<FTransform>& OutGlobals) const;
	void GetCurrentBoneGlobalMatrices(TArray<FMatrix>& OutGlobals) const;
	const TArray<FVertexPNCTT>& GetSkinnedVertices() const { return SkinnedVertices; }
	uint64 GetSkinnedRevision() const { return SkinnedRevision; }
	FMeshBuffer* GetMeshBuffer() const override;
	FMeshDataView GetMeshDataView() const override;

protected:
	// Tick/skinning 섹션: animation system 없이 현재 bone edit pose를 매 frame CPU skinning 결과로 반영한다.
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	void InitSkinningCache();
	void UpdateCPUSkinning();
	void BuildBoneEditGlobalTransforms(TArray<FTransform>& OutGlobals) const;
	void BuildBoneEditGlobalMatrices(TArray<FMatrix>& OutGlobals) const;

protected:
	// Mesh/material state는 SetSkeletalMesh와 PostEditProperty가 같은 경로를 쓰도록 여기서 소유한다.
	USkeletalMesh* SkeletalMesh = nullptr;
	FString SkeletalMeshPath = "None";
	TArray<UMaterial*> OverrideMaterials;
	TArray<FMaterialSlot> MaterialSlots;

	// Bone edit pose는 asset 원본 bone을 직접 바꾸지 않고 component-local override로만 유지한다.
	TArray<FMatrix> BoneEditLocalMatrices;
	bool bUseBoneEditPose = false;

	// SceneProxy는 이 결과와 revision만 보고 dynamic vertex buffer를 갱신한다.
	TArray<FVertexPNCTT> SkinnedVertices;
	uint64 SkinnedRevision = 0;
};
