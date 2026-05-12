#include "SkeletalMeshComponent.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Mesh/SkeletalMesh.h"

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

namespace
{
	constexpr float MatrixDecomposeTolerance = 1.0e-6f;

	FTransform MatrixToEditorTransform(const FMatrix& Matrix)
	{
		FTransform Result;
		Result.Location = Matrix.GetLocation();
		Result.Scale = Matrix.GetScale();

		FMatrix RotationMatrix = Matrix;
		RotationMatrix.M[3][0] = 0.0f;
		RotationMatrix.M[3][1] = 0.0f;
		RotationMatrix.M[3][2] = 0.0f;
		RotationMatrix.M[3][3] = 1.0f;

		if (std::fabs(Result.Scale.X) > MatrixDecomposeTolerance)
		{
			RotationMatrix.M[0][0] /= Result.Scale.X;
			RotationMatrix.M[0][1] /= Result.Scale.X;
			RotationMatrix.M[0][2] /= Result.Scale.X;
		}

		if (std::fabs(Result.Scale.Y) > MatrixDecomposeTolerance)
		{
			RotationMatrix.M[1][0] /= Result.Scale.Y;
			RotationMatrix.M[1][1] /= Result.Scale.Y;
			RotationMatrix.M[1][2] /= Result.Scale.Y;
		}

		if (std::fabs(Result.Scale.Z) > MatrixDecomposeTolerance)
		{
			RotationMatrix.M[2][0] /= Result.Scale.Z;
			RotationMatrix.M[2][1] /= Result.Scale.Z;
			RotationMatrix.M[2][2] /= Result.Scale.Z;
		}

		Result.Rotation = RotationMatrix.ToQuat().GetNormalized();
		return Result;
	}

	float SafeScaleDivide(float Numerator, float Denominator)
	{
		return std::fabs(Denominator) > MatrixDecomposeTolerance ? Numerator / Denominator : Numerator;
	}

	FVector SafeScaleDivide(const FVector& Numerator, const FVector& Denominator)
	{
		return FVector(
			SafeScaleDivide(Numerator.X, Denominator.X),
			SafeScaleDivide(Numerator.Y, Denominator.Y),
			SafeScaleDivide(Numerator.Z, Denominator.Z));
	}
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
	USkinnedMeshComponent::SetSkeletalMesh(InMesh);
	InitSkinningCache();
	// 새 mesh로 바뀌면 기존 bone edit pose를 갱신한다.
	BoneEditLocalMatrices.clear();
	bUseBoneEditPose = false;

	if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshAsset())
	{
		ResetBoneEditPose();
	}

	CacheLocalBounds();
	// MarkRenderStateDirty();
	MarkWorldBoundsDirty();
	/*MarkProxyDirty(EDirtyFlag::Mesh);
	MarkProxyDirty(EDirtyFlag::Material);*/
	// 강제로 Update
	// TODO: 원인 파악 반드시 해야함
	SceneProxy->UpdateMesh();
	SceneProxy->UpdateMaterial();
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
		BoneEditLocalMatrices.push_back(Bone.LocalTransform.ToMatrix());
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
		BoneEditLocalMatrices.push_back(Bone.LocalTransform.ToMatrix());
	}
}

FVector USkeletalMeshComponent::GetBoneLocationByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FVector::ZeroVector;

	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	const FVector ComponentLocalLocation = GlobalMatrices[BoneIndex].GetLocation();
	return GetWorldMatrix().TransformPositionWithW(ComponentLocalLocation);
}

FRotator USkeletalMeshComponent::GetBoneRotationByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FRotator::ZeroRotator;

	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	const FMatrix BoneWorldMatrix = GlobalMatrices[BoneIndex] * GetWorldMatrix();
	return MatrixToEditorTransform(BoneWorldMatrix).Rotation.ToRotator();
}

FQuat USkeletalMeshComponent::GetBoneQuatByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FQuat::Identity;

	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	const FMatrix BoneWorldMatrix = GlobalMatrices[BoneIndex] * GetWorldMatrix();
	return MatrixToEditorTransform(BoneWorldMatrix).Rotation;
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

FTransform USkeletalMeshComponent::GetBoneLocalTransformByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FMatrix::Identity;

	if (bUseBoneEditPose && BoneEditLocalMatrices.size() == Asset->Bones.size()) return MatrixToEditorTransform(BoneEditLocalMatrices[BoneIndex]);

	return Asset->Bones[BoneIndex].LocalTransform.ToMatrix();
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

	FMatrix DesiredGlobalMatrix = GlobalMatrices[BoneIndex];
	DesiredGlobalMatrix.SetLocation(DesiredComponentLocalLocation);

	const int32 ParentIndex = Asset->Bones[BoneIndex].ParentIndex;
	if (ParentIndex >= 0)
	{
		const FMatrix ParentGlobalInv = GlobalMatrices[ParentIndex].GetInverse();
		BoneEditLocalMatrices[BoneIndex] = DesiredGlobalMatrix * ParentGlobalInv;
	}
	else
	{
		BoneEditLocalMatrices[BoneIndex] = DesiredGlobalMatrix;
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

	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	const FQuat ComponentWorldQuat = MatrixToEditorTransform(GetWorldMatrix()).Rotation;
	const FQuat ComponentWorldQuatInv = ComponentWorldQuat.Inverse();

	const FQuat DesiredWorldQuat = NewRotation.ToQuaternion().GetNormalized();
	const FQuat DesiredComponentGlobalQuat = (DesiredWorldQuat * ComponentWorldQuatInv).GetNormalized();

	FTransform DesiredGlobal = MatrixToEditorTransform(GlobalMatrices[BoneIndex]);
	DesiredGlobal.Rotation = DesiredComponentGlobalQuat;
	const FMatrix DesiredGlobalMatrix = DesiredGlobal.ToMatrix();

	const int32 ParentIndex = Asset->Bones[BoneIndex].ParentIndex;
	if (ParentIndex >= 0)
	{
		const FMatrix ParentGlobalInv = GlobalMatrices[ParentIndex].GetInverse();
		BoneEditLocalMatrices[BoneIndex] = DesiredGlobalMatrix * ParentGlobalInv;
	}
	else
	{
		BoneEditLocalMatrices[BoneIndex] = DesiredGlobalMatrix;
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

	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	const FQuat ComponentWorldQuat = MatrixToEditorTransform(GetWorldMatrix()).Rotation;
	const FQuat ComponentWorldQuatInv = ComponentWorldQuat.Inverse();

	const FQuat DesiredWorldQuat = NewQuat.GetNormalized();
	const FQuat DesiredComponentGlobalQuat = (DesiredWorldQuat * ComponentWorldQuatInv).GetNormalized();

	FTransform DesiredGlobal = MatrixToEditorTransform(GlobalMatrices[BoneIndex]);
	DesiredGlobal.Rotation = DesiredComponentGlobalQuat;
	const FMatrix DesiredGlobalMatrix = DesiredGlobal.ToMatrix();

	const int32 ParentIndex = Asset->Bones[BoneIndex].ParentIndex;
	if (ParentIndex >= 0)
	{
		const FMatrix ParentGlobalInv = GlobalMatrices[ParentIndex].GetInverse();
		BoneEditLocalMatrices[BoneIndex] = DesiredGlobalMatrix * ParentGlobalInv;
	}
	else
	{
		BoneEditLocalMatrices[BoneIndex] = DesiredGlobalMatrix;
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

	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	const FVector ComponentWorldScale = MatrixToEditorTransform(GetWorldMatrix()).Scale;
	const FVector DesiredComponentGlobalScale = SafeScaleDivide(NewScale, ComponentWorldScale);

	FTransform DesiredGlobal = MatrixToEditorTransform(GlobalMatrices[BoneIndex]);
	DesiredGlobal.Scale = DesiredComponentGlobalScale;
	const FMatrix DesiredGlobalMatrix = DesiredGlobal.ToMatrix();

	const int32 ParentIndex = Asset->Bones[BoneIndex].ParentIndex;
	if (ParentIndex >= 0)
	{
		const FMatrix ParentGlobalInv = GlobalMatrices[ParentIndex].GetInverse();
		BoneEditLocalMatrices[BoneIndex] = DesiredGlobalMatrix * ParentGlobalInv;
	}
	else
	{
		BoneEditLocalMatrices[BoneIndex] = DesiredGlobalMatrix;
	}

	bUseBoneEditPose = true;
	UpdateCPUSkinning();
	MarkWorldBoundsDirty();
}

void USkeletalMeshComponent::SetBoneLocalTransformByIndex(int32 BoneIndex, const FTransform& NewLocalTransform)
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return;

	EnsureBoneEditPose();
	BoneEditLocalMatrices[BoneIndex] = NewLocalTransform.ToMatrix();

	bUseBoneEditPose = true;
	UpdateCPUSkinning();
	MarkWorldBoundsDirty();
}

void USkeletalMeshComponent::GetCurrentBoneGlobalTransforms(TArray<FTransform>& OutGlobals) const
{
	BuildBoneEditGlobalTransforms(OutGlobals);
}

void USkeletalMeshComponent::GetCurrentBoneGlobalMatrices(TArray<FMatrix>& OutGlobals) const
{
	BuildBoneEditGlobalMatrices(OutGlobals);
}

void USkeletalMeshComponent::BuildBoneEditGlobalTransforms(TArray<FTransform>& OutGlobals) const
{
	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	OutGlobals.clear();
	OutGlobals.reserve(GlobalMatrices.size());
	for (const FMatrix& GlobalMatrix : GlobalMatrices)
	{
		OutGlobals.push_back(MatrixToEditorTransform(GlobalMatrix));
	}
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
		const FMatrix LocalMatrix = (bUseBoneEditPose && BoneEditLocalMatrices.size() == BoneCount)
			? BoneEditLocalMatrices[i] : Asset->Bones[i].LocalTransform.ToMatrix();

		const int32 ParentIndex = Asset->Bones[i].ParentIndex;
		OutGlobals[i] = (ParentIndex >= 0) ? LocalMatrix * OutGlobals[ParentIndex] : LocalMatrix;
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

	TArray<FMatrix> BoneGlobals;
	GetCurrentBoneGlobalMatrices(BoneGlobals);

	auto SkinVertexRange = [&](uint32 VertexStart, uint32 VertexEnd, const FMatrix& MeshBindGlobal)
		{
			TArray<FMatrix> SkinMatrices;
			SkinMatrices.resize(Asset->Bones.size(), FMatrix::Identity);

			for (int32 BoneIndex = 0; BoneIndex < (int32)Asset->Bones.size(); ++BoneIndex)
			{
				if (BoneIndex < static_cast<int32>(BoneGlobals.size()))
				{
					SkinMatrices[BoneIndex] =
						MeshBindGlobal * Asset->Bones[BoneIndex].InverseBindPoseMatrix * BoneGlobals[BoneIndex];
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
}

void USkeletalMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	USkinnedMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateCPUSkinning();
}
