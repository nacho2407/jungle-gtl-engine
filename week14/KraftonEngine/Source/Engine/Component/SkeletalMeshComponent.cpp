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
		BoneEditLocalTransforms.clear();
		bUseBoneEditPose = false;
		return;
	}

	if (BoneEditLocalTransforms.size() == Asset->Bones.size()) return;

	BoneEditLocalTransforms.clear();
	BoneEditLocalTransforms.reserve(Asset->Bones.size());

	for (const FBone& Bone : Asset->Bones)
	{
		BoneEditLocalTransforms.push_back(Bone.LocalTransform);
	}

	bUseBoneEditPose = true;
}

void USkeletalMeshComponent::ResetBoneEditPose()
{
	BoneEditLocalTransforms.clear();
	bUseBoneEditPose = false;

	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset) return;

	BoneEditLocalTransforms.reserve(Asset->Bones.size());
	for (const FBone& Bone : Asset->Bones)
	{
		BoneEditLocalTransforms.push_back(Bone.LocalTransform);
	}
}

FVector USkeletalMeshComponent::GetBoneLocationByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FVector::ZeroVector;

	TArray<FTransform> GlobalTransforms;
	BuildBoneEditGlobalTransforms(GlobalTransforms);

	const FVector ComponentLocalLocation = GlobalTransforms[BoneIndex].Location;
	return GetWorldMatrix().TransformPositionWithW(ComponentLocalLocation);
}

FRotator USkeletalMeshComponent::GetBoneRotationByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FRotator::ZeroRotator;

	TArray<FTransform> GlobalTransforms;
	BuildBoneEditGlobalTransforms(GlobalTransforms);

	const FMatrix BoneWorldMatrix = GlobalTransforms[BoneIndex].ToMatrix() * GetWorldMatrix();
	return BoneWorldMatrix.ToQuat().GetNormalized().ToRotator();
}

FVector USkeletalMeshComponent::GetBoneScaleByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FVector::ZeroVector;

	TArray<FTransform> GlobalTransforms;
	BuildBoneEditGlobalTransforms(GlobalTransforms);

	const FMatrix BoneWorldMatrix = GlobalTransforms[BoneIndex].ToMatrix() * GetWorldMatrix();
	return BoneWorldMatrix.GetScale();
}

FTransform USkeletalMeshComponent::GetBoneLocalTransformByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FMatrix::Identity;

	if (bUseBoneEditPose && BoneEditLocalTransforms.size() == Asset->Bones.size()) return BoneEditLocalTransforms[BoneIndex];

	return Asset->Bones[BoneIndex].LocalTransform.ToMatrix();
}

void USkeletalMeshComponent::SetBoneLocationByIndex(int32 BoneIndex, const FVector& NewLocation)
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return;

	EnsureBoneEditPose();

	TArray<FTransform> GlobalTransforms;
	BuildBoneEditGlobalTransforms(GlobalTransforms);

	const FMatrix ComponentWorldInv = GetWorldMatrix().GetInverse();
	const FVector DesiredComponentLocalLocation = ComponentWorldInv.TransformPositionWithW(NewLocation);

	FMatrix DesiredGlobal = GlobalTransforms[BoneIndex].ToMatrix();
	DesiredGlobal.SetLocation(DesiredComponentLocalLocation);

	const int32 ParentIndex = Asset->Bones[BoneIndex].ParentIndex;
	if (ParentIndex >= 0)
	{
		const FMatrix ParentGlobalInv = GlobalTransforms[ParentIndex].ToMatrix().GetInverse();
		BoneEditLocalTransforms[BoneIndex] = FTransform(DesiredGlobal * ParentGlobalInv);
	}
	else
	{
		BoneEditLocalTransforms[BoneIndex] = FTransform(DesiredGlobal);
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

	TArray<FTransform> GlobalTransforms;
	BuildBoneEditGlobalTransforms(GlobalTransforms);

	const FQuat ComponentWorldQuat = GetWorldMatrix().ToQuat().GetNormalized();
	const FQuat ComponentWorldQuatInv = ComponentWorldQuat.Inverse();

	const FQuat DesiredComponentLocalQuat = ComponentWorldQuat * NewRotation.ToQuaternion();

	FTransform DesiredGlobal = GlobalTransforms[BoneIndex];
	DesiredGlobal.Rotation = DesiredComponentLocalQuat.GetNormalized();

	const int32 ParentIndex = Asset->Bones[BoneIndex].ParentIndex;
	if (ParentIndex >= 0)
	{
		const FMatrix ParentGlobalInv = GlobalTransforms[ParentIndex].ToMatrix().GetInverse();
		BoneEditLocalTransforms[BoneIndex] = FTransform(DesiredGlobal.ToMatrix() * ParentGlobalInv);
	}
	else
	{
		BoneEditLocalTransforms[BoneIndex] = DesiredGlobal;
	}

	bUseBoneEditPose = true;
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

void USkeletalMeshComponent::SetBoneScaleByIndex(int32 BoneIndex, const FVector& NewScale)
{
}

void USkeletalMeshComponent::SetBoneLocalTransformByIndex(int32 BoneIndex, const FTransform& NewLocalTransform)
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return;

	EnsureBoneEditPose();
	BoneEditLocalTransforms[BoneIndex] = NewLocalTransform;

	bUseBoneEditPose = true;
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

void USkeletalMeshComponent::GetCurrentBoneGlobalTransforms(TArray<FTransform>& OutGlobals) const
{
	BuildBoneEditGlobalTransforms(OutGlobals);
}

void USkeletalMeshComponent::BuildBoneEditGlobalTransforms(TArray<FTransform>& OutGlobals) const
{
	OutGlobals.clear(); 

	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset) return;

	const int32 BoneCount = static_cast<int32>(Asset->Bones.size());
	OutGlobals.resize(BoneCount);

	for (int32 i = 0; i < BoneCount; ++i)
	{
		const FTransform Local = (bUseBoneEditPose && BoneEditLocalTransforms.size() == BoneCount) 
			? BoneEditLocalTransforms[i] : Asset->Bones[i].LocalTransform;

		const int32 ParentIndex = Asset->Bones[i].ParentIndex;
		FMatrix GlobalMatrix = (ParentIndex >= 0) ? Local.ToMatrix() * OutGlobals[ParentIndex].ToMatrix() : Local.ToMatrix();
		OutGlobals[i] = FTransform(GlobalMatrix);
	}
}
