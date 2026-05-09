#include "SkeletalMeshComponent.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Mesh/SkeletalMesh.h"

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

FMeshBuffer* USkeletalMeshComponent::GetMeshBuffer() const
{
	if (!SkeletalMesh) return nullptr;
	FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
	if (!Asset || !Asset->RenderBuffer) return nullptr;
	return Asset->RenderBuffer.get();
}

FMeshDataView USkeletalMeshComponent::GetMeshDataView() const
{
	if (!SkeletalMesh) return {};
	FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
	if (!Asset || Asset->Vertices.empty()) return {};

	FMeshDataView View;
	View.VertexData = Asset->Vertices.data();
	View.VertexCount = (uint32)Asset->Vertices.size();
	View.Stride = sizeof(FVertexPNCTBW);
	View.IndexData = Asset->Indices.data();
	View.IndexCount = (uint32)Asset->Indices.size();
	return View;
}

FPrimitiveSceneProxy* USkeletalMeshComponent::CreateSceneProxy()
{
	return new FSkeletalMeshSceneProxy(this);
}

void USkeletalMeshComponent::EnsureBoneEditPose()
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset)
	{
		BoneEditLocalMatrices.clear();
		bUseBoneEditPose = false;
		return;
	}

	if (BoneEditLocalMatrices.size() == Asset->Bones.size()) return;

	BoneEditLocalMatrices.clear();
	BoneEditLocalMatrices.reserve(Asset->Bones.size());

	for (const FBone& Bone : Asset->Bones)
	{
		BoneEditLocalMatrices.push_back(Bone.LocalMatrix);
	}

	bUseBoneEditPose = true;
}

void USkeletalMeshComponent::ResetBoneEditPose()
{
	BoneEditLocalMatrices.clear();
	bUseBoneEditPose = false;

	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset) return;

	BoneEditLocalMatrices.reserve(Asset->Bones.size());
	for (const FBone& Bone : Asset->Bones)
	{
		BoneEditLocalMatrices.push_back(Bone.LocalMatrix);
	}
}

FVector USkeletalMeshComponent::GetBoneLocationByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FVector::ZeroVector;

	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	const FVector ComponentLocalLocation = Asset->Bones[BoneIndex].GlobalMatrix.GetLocation();
	return GetWorldMatrix().TransformPositionWithW(ComponentLocalLocation);
}

FRotator USkeletalMeshComponent::GetBoneRotationByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FRotator::ZeroRotator;

	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	const FMatrix BoneWorldMatrix = GlobalMatrices[BoneIndex] * GetWorldMatrix();
	return BoneWorldMatrix.ToQuat().GetNormalized().ToRotator();
}

FVector USkeletalMeshComponent::GetBoneScaleByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FVector::ZeroVector;

	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	const FMatrix BoneWorldMatrix = GlobalMatrices[BoneIndex] * GetWorldMatrix();
	return BoneWorldMatrix.GetScale();
}

void USkeletalMeshComponent::SetBoneLocationByIndex(int32 BoneIndex, const FVector& NewLocation)
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return;

	EnsureBoneEditPose();

	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	const FMatrix ComponentWorldInv = GetWorldMatrix().GetInverse();
	const FVector DesiredComponentLocalLocation = ComponentWorldInv.TransformPositionWithW(NewLocation);

	FMatrix DesiredGlobal = GlobalMatrices[BoneIndex];
	DesiredGlobal.SetLocation(DesiredComponentLocalLocation);

	const int32 ParentIndex = Asset->Bones[BoneIndex].ParentIndex;
	if (ParentIndex >= 0)
	{
		const FMatrix ParentGlobalInv = GlobalMatrices[ParentIndex].GetInverse();
		BoneEditLocalMatrices[BoneIndex] = DesiredGlobal * ParentGlobalInv;
	}
	else
	{
		BoneEditLocalMatrices[BoneIndex] = DesiredGlobal;
	}

	bUseBoneEditPose = true;
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

void USkeletalMeshComponent::SetBoneRotationByIndex(int32 BoneIndex, const FRotator& NewRotation)
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return;

	EnsureBoneEditPose();

	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	const FQuat ComponentWorldQuat = GetWorldMatrix().ToQuat().GetNormalized();
	const FQuat ComponentWorldQuatInv = ComponentWorldQuat.Inverse();


}

void USkeletalMeshComponent::SetBoneScaleByIndex(int32 BoneIndex, const FVector& NewScale)
{
}

void USkeletalMeshComponent::BuildBoneEditGlobalMatrices(TArray<FMatrix>& OutGlobals) const
{
	OutGlobals.clear(); 

	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset) return;

	const int32 BoneCount = static_cast<int32>(Asset->Bones.size());
	OutGlobals.resize(BoneCount);

	for (int32 i = 0; i < BoneCount; ++i)
	{
		const FMatrix Local = (bUseBoneEditPose && BoneEditLocalMatrices.size() == BoneCount) 
			? BoneEditLocalMatrices[i] : Asset->Bones[i].LocalMatrix;

		const int32 ParentIndex = Asset->Bones[i].ParentIndex;
		OutGlobals[i] = (ParentIndex >= 0) ? Local * OutGlobals[ParentIndex] : Local;
	}
}
