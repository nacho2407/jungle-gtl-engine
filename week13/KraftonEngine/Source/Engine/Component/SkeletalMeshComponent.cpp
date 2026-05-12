#include "SkeletalMeshComponent.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Mesh/SkeletalMesh.h"

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
	USkinnedMeshComponent::SetSkeletalMesh(InMesh);
	InitSkinningCache();
	// 새 mesh로 바뀌면 기존 bone edit pose를 갱신한다.
	BoneEditLocalTransforms.clear();
	bUseBoneEditPose = false;

	if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshAsset())
	{
		// 1. BoneEditLocalTransforms을 InMesh 기준으로 다시 갱신한다.
		ResetBoneEditPose();

		// 2. 실제 그릴 정점 생성
		// UpdateCPUSkinning();
	}

	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

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

//void USkeletalMeshComponent::UpdateWorldAABB() const
//{
//	if (SkinnedVertices.empty())
//	{
//		USkinnedMeshComponent::UpdateWorldAABB();
//		return;
//	}
//
//	const FMatrix& WorldMatrix = CachedWorldMatrix;
//
//	FVector WorldMin = WorldMatrix.TransformPositionWithW(SkinnedVertices[0].Position);
//	FVector WorldMax = WorldMin;
//
//	for (const FVertexPNCTT& Vertex : SkinnedVertices)
//	{
//		const FVector WorldPos = WorldMatrix.TransformPositionWithW(Vertex.Position);
//
//		WorldMin.X = std::min(WorldMin.X, WorldPos.X);
//		WorldMin.Y = std::min(WorldMin.Y, WorldPos.Y);
//		WorldMin.Z = std::min(WorldMin.Z, WorldPos.Z);
//
//		WorldMax.X = std::max(WorldMax.X, WorldPos.X);
//		WorldMax.Y = std::max(WorldMax.Y, WorldPos.Y);
//		WorldMax.Z = std::max(WorldMax.Z, WorldPos.Z);
//	}
//
//	constexpr float MinExtent = 1.0f;
//
//	FVector Center = (WorldMin + WorldMax) * 0.5f;
//	FVector Extent = (WorldMax - WorldMin) * 0.5f;
//
//	Extent.X = std::max(Extent.X, MinExtent);
//	Extent.Y = std::max(Extent.Y, MinExtent);
//	Extent.Z = std::max(Extent.Z, MinExtent);
//
//	WorldAABBMinLocation = Center - Extent;
//	WorldAABBMaxLocation = Center + Extent;
//
//	bWorldAABBDirty = false;
//	bHasValidWorldAABB = true;
//}

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

FQuat USkeletalMeshComponent::GetBoneQuatByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FQuat::Identity;

	TArray<FTransform> GlobalTransforms;
	BuildBoneEditGlobalTransforms(GlobalTransforms);

	const FMatrix BoneWorldMatrix = GlobalTransforms[BoneIndex].ToMatrix() * GetWorldMatrix();
	return BoneWorldMatrix.ToQuat().GetNormalized();
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

	FTransform DesiredGlobal = GlobalTransforms[BoneIndex];
	DesiredGlobal.Location = DesiredComponentLocalLocation;

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
	UpdateCPUSkinning();
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

	const FQuat DesiredWorldQuat = NewRotation.ToQuaternion().GetNormalized();
	const FQuat DesiredComponentGlobalQuat = (DesiredWorldQuat * ComponentWorldQuatInv).GetNormalized();

	FTransform DesiredGlobal = GlobalTransforms[BoneIndex];
	DesiredGlobal.Rotation = DesiredComponentGlobalQuat;

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
	UpdateCPUSkinning();
	MarkWorldBoundsDirty();
}

void USkeletalMeshComponent::SetBoneRotationByIndex(int32 BoneIndex, const FQuat& NewQuat)
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return;

	EnsureBoneEditPose();

	TArray<FTransform> GlobalTransforms;
	BuildBoneEditGlobalTransforms(GlobalTransforms);

	const FQuat ComponentWorldQuat = GetWorldMatrix().ToQuat().GetNormalized();
	const FQuat ComponentWorldQuatInv = ComponentWorldQuat.Inverse();

	const FQuat DesiredWorldQuat = NewQuat.GetNormalized();
	const FQuat DesiredComponentGlobalQuat = (DesiredWorldQuat * ComponentWorldQuatInv).GetNormalized();

	FTransform DesiredGlobal = GlobalTransforms[BoneIndex];
	DesiredGlobal.Rotation = DesiredComponentGlobalQuat;

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
	UpdateCPUSkinning();
	MarkWorldBoundsDirty();
}

void USkeletalMeshComponent::SetBoneScaleByIndex(int32 BoneIndex, const FVector& NewScale)
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return;

	EnsureBoneEditPose();

	FTransform LocalTransform = BoneEditLocalTransforms[BoneIndex];
	LocalTransform.Scale = NewScale;

	BoneEditLocalTransforms[BoneIndex] = LocalTransform;

	bUseBoneEditPose = true;
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

void USkeletalMeshComponent::SetBoneLocalTransformByIndex(int32 BoneIndex, const FTransform& NewLocalTransform)
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return;

	EnsureBoneEditPose();
	BoneEditLocalTransforms[BoneIndex] = NewLocalTransform;

	bUseBoneEditPose = true;
	UpdateCPUSkinning();
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

void USkeletalMeshComponent::InitSkinningCache()
{
	USkeletalMesh* Mesh = GetSkeletalMesh();
	if (!Mesh || !Mesh->GetSkeletalMeshAsset())
	{
		SkinnedVertices.clear();
		++SkinnedRevision;
		return;
	}

	FSkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
	SkinnedVertices.resize(Asset->Vertices.size());
	UpdateCPUSkinning();
	// UpdateWorldAABB();
}

void USkeletalMeshComponent::UpdateCPUSkinning()
{
	USkeletalMesh* Mesh = GetSkeletalMesh();
	if (!Mesh || !Mesh->GetSkeletalMeshAsset())
	{
		SkinnedVertices.clear();
		++SkinnedRevision;
		return;
	}

	FSkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
	if (Asset->Vertices.empty())
	{
		SkinnedVertices.clear();
		++SkinnedRevision;
		return;
	}

	if (SkinnedVertices.size() != Asset->Vertices.size())
	{
		SkinnedVertices.resize(Asset->Vertices.size());
	}

	TArray<FTransform> BoneGlobals;
	GetCurrentBoneGlobalTransforms(BoneGlobals);

	auto SkinVertexRange = [&](uint32 VertexStart, uint32 VertexEnd, const FMatrix& MeshBindGlobal)
		{
			TArray<FMatrix> SkinMatrices;
			SkinMatrices.resize(Asset->Bones.size(), FMatrix::Identity);

			for (int32 BoneIndex = 0; BoneIndex < (int32)Asset->Bones.size(); ++BoneIndex)
			{
				if (BoneIndex < static_cast<int32>(BoneGlobals.size()))
				{
					SkinMatrices[BoneIndex] =
						MeshBindGlobal * Asset->Bones[BoneIndex].InverseBindPoseMatrix * BoneGlobals[BoneIndex].ToMatrix();
				}
			}

			VertexEnd = std::min<uint32>(VertexEnd, (uint32)Asset->Vertices.size());
			for (uint32 i = VertexStart; i < VertexEnd; ++i)
			{
				const FVertexPNCTBW& Src = Asset->Vertices[i];
				FVertexPNCTT& Dst = SkinnedVertices[i];

				FVector SkinnedPos = FVector::ZeroVector;
				FVector SkinnedNormal = FVector::ZeroVector;
				FVector SkinnedTangent = FVector::ZeroVector;
				float AccumWeight = 0.0f;

				for (int32 k = 0; k < 4; ++k)
				{
					const int32 BoneIndex = Src.BoneIndices[k];
					const float Weight = Src.BoneWeights[k];

					if (Weight <= 0.0f) continue;
					if (BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) continue;

					const FMatrix& M = SkinMatrices[BoneIndex];

					SkinnedPos += M.TransformPositionWithW(Src.Position) * Weight;
					SkinnedNormal += M.TransformVector(Src.Normal) * Weight;
					SkinnedTangent += M.TransformVector(FVector(Src.Tangent.X, Src.Tangent.Y, Src.Tangent.Z)) * Weight;
					AccumWeight += Weight;
				}

				if (AccumWeight <= 0.0f)
				{
					SkinnedPos = MeshBindGlobal.TransformPositionWithW(Src.Position);
					SkinnedNormal = MeshBindGlobal.TransformVector(Src.Normal);
					SkinnedTangent = MeshBindGlobal.TransformVector(FVector(Src.Tangent.X, Src.Tangent.Y, Src.Tangent.Z));
					if (!SkinnedNormal.IsNearlyZero())
					{
						SkinnedNormal.Normalize();
					}
				}
				else if (!SkinnedNormal.IsNearlyZero())
				{
					SkinnedNormal.Normalize();
				}

				if (!SkinnedTangent.IsNearlyZero())
				{
					SkinnedTangent.Normalize();
				}
				else
				{
					SkinnedTangent = FVector(1.0f, 0.0f, 0.0f);
				}

				Dst.Position = SkinnedPos;
				Dst.Normal = SkinnedNormal;
				Dst.Color = Src.Color;
				Dst.UV = Src.UV;
				Dst.Tangent = FVector4(SkinnedTangent, Src.Tangent.W);
			}
		};

	if (!Asset->MeshRanges.empty())
	{
		for (const FSkeletalMeshRange& Range : Asset->MeshRanges)
		{
			SkinVertexRange(Range.VertexStart, Range.VertexEnd, Range.MeshBindGlobal);
		}
	}
	else
	{
		SkinVertexRange(0, (uint32)Asset->Vertices.size(), FMatrix::Identity);
	}

	++SkinnedRevision;
	// UpdateWorldAABB();
}

void USkeletalMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	USkinnedMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateCPUSkinning();
}
