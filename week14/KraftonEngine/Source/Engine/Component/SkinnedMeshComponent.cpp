#include "SkinnedMeshComponent.h"
#include "Mesh/SkeletalMesh.h"
#include "Serialization/Archive.h"
#include "Runtime/Engine.h"
#include "Mesh/MeshManager.h"
#include "Collision/RayUtils.h"
#include "Core/Log.h"

IMPLEMENT_CLASS(USkinnedMeshComponent, UMeshComponent)
HIDE_FROM_COMPONENT_LIST(USkinnedMeshComponent)

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

// SkeletalMesh 교체는 표시 여부, material slot, CPU skinning, bounds dirty가 모두 엮여 있다.
// 그래서 하위 SkeletalMeshComponent가 아니라 여기서 전체 순서를 고정해 중복 dirty 등록을 막는다.
void USkinnedMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
	// 먼저 pointer/path/material slot을 맞춰 editor와 runtime이 같은 mesh 상태를 보게 한다.
	SkeletalMesh = InMesh;

	if (InMesh)
	{
		SkeletalMeshPath = InMesh->GetAssetPathFileName();
		const TArray<FSkeletalMaterial>& DefaultMaterials = SkeletalMesh->GetSkeletalMaterials();

		OverrideMaterials.resize(DefaultMaterials.size());
		MaterialSlots.resize(DefaultMaterials.size());

		for (int32 i = 0; i < (int32)DefaultMaterials.size(); ++i)
		{
			OverrideMaterials[i] = DefaultMaterials[i].MaterialInterface;

			if (OverrideMaterials[i])
				MaterialSlots[i].Path = OverrideMaterials[i]->GetAssetPathFileName();
			else
				MaterialSlots[i].Path = "None";
		}
	}
	else
	{
		SkeletalMeshPath = "None";
		OverrideMaterials.clear();
		MaterialSlots.clear();
	}

	// Mesh가 바뀌면 이전 bone edit pose는 새 skeleton과 index 호환을 보장할 수 없다.
	BoneEditLocalMatrices.clear();
	bUseBoneEditPose = false;

	// SceneProxy가 즉시 그릴 수 있도록 SetSkeletalMesh 종료 전에 skinned vertex buffer를 준비한다.
	InitSkinningCache();

	if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshAsset())
	{
		ResetBoneEditPose();
		UpdateCPUSkinning();
	}
	else
	{
		SkinnedVertices.clear();
		++SkinnedRevision;
	}

	// 최종 dirty 처리는 여기서만 수행해 PostEditProperty/PostDuplicate의 중복 등록을 피한다.
	// MarkRenderStateDirty();
	// TODO: MarkRenderStateDirty를 수행하면 Proxy가 없어졌다가 생성된다.
	// 근데 원인 불명의 이유로 Octree에 추가가 안되서 최초 시점에 렌더링이 안된다.
	// 우선 임시 방편으로 MarkProxyDirty로 Mesh와 Material에 DirtyFlag를 갱신하고 추후 수정하는 방향으로 간다.
	MarkProxyDirty(EDirtyFlag::Mesh);
	MarkProxyDirty(EDirtyFlag::Material);
	MarkWorldBoundsDirty();
}

USkeletalMesh* USkinnedMeshComponent::GetSkeletalMesh() const
{
	return SkeletalMesh;
}

// Bounds 섹션: SkeletalMesh culling은 asset local bounds가 아니라 실제 CPU-skinned vertices를 기준으로 한다.
void USkinnedMeshComponent::UpdateWorldAABB() const
{
	// 아직 skinning 결과가 없으면 primitive 기본 bounds로 fallback해 빈 mesh/로드 실패 경로를 안전하게 둔다.
	if (SkinnedVertices.empty())
	{
		UPrimitiveComponent::UpdateWorldAABB();
		return;
	}

	const FMatrix& WorldMatrix = CachedWorldMatrix;

	// 이미 component local로 skinning된 vertex를 world matrix로 변환해 octree/query bounds를 만든다.
	FVector WorldMin = WorldMatrix.TransformPositionWithW(SkinnedVertices[0].Position);
	FVector WorldMax = WorldMin;

	for (const FVertexPNCTT& Vertex : SkinnedVertices)
	{
		const FVector WorldPos = WorldMatrix.TransformPositionWithW(Vertex.Position);

		WorldMin.X = std::min(WorldMin.X, WorldPos.X);
		WorldMin.Y = std::min(WorldMin.Y, WorldPos.Y);
		WorldMin.Z = std::min(WorldMin.Z, WorldPos.Z);

		WorldMax.X = std::max(WorldMax.X, WorldPos.X);
		WorldMax.Y = std::max(WorldMax.Y, WorldPos.Y);
		WorldMax.Z = std::max(WorldMax.Z, WorldPos.Z);
	}

	FVector Center = (WorldMin + WorldMax) * 0.5f;
	FVector Extent = (WorldMax - WorldMin) * 0.5f;

	WorldAABBMinLocation = Center - Extent;
	WorldAABBMaxLocation = Center + Extent;
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

// Bone edit 섹션: setter가 호출되기 전까지는 asset pose를 그대로 쓰고, 수정 순간에 component-local 복사본을 만든다.
void USkinnedMeshComponent::EnsureBoneEditPose()
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset)
	{
		BoneEditLocalMatrices.clear();
		bUseBoneEditPose = false;
		return;
	}

	// bone count가 같으면 현재 edit pose를 유지해야 사용자가 조작한 값을 잃지 않는다.
	if (BoneEditLocalMatrices.size() == Asset->Bones.size()) return;

	BoneEditLocalMatrices.clear();
	BoneEditLocalMatrices.reserve(Asset->Bones.size());

	for (const FBone& Bone : Asset->Bones)
	{
		BoneEditLocalMatrices.push_back(Bone.LocalMatrix);
	}

	bUseBoneEditPose = true;
}

// Reset은 mesh 교체 직후 asset의 기본 pose를 기준으로 CPU skinning을 안정적으로 시작하기 위한 경로다.
void USkinnedMeshComponent::ResetBoneEditPose()
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

FVector USkinnedMeshComponent::GetBoneLocationByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FVector::ZeroVector;

	// 외부 API는 world space 값을 기대하므로 component-local global matrix를 world matrix로 변환한다.
	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	const FVector ComponentLocalLocation = GlobalMatrices[BoneIndex].GetLocation();
	return GetWorldMatrix().TransformPositionWithW(ComponentLocalLocation);
}

FRotator USkinnedMeshComponent::GetBoneRotationByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FRotator::ZeroRotator;

	// parent hierarchy를 반영한 bone global에 component world rotation을 더해 world rotation으로 반환한다.
	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	const FMatrix BoneWorldMatrix = GlobalMatrices[BoneIndex] * GetWorldMatrix();
	return MatrixToEditorTransform(BoneWorldMatrix).Rotation.ToRotator();
}

FQuat USkinnedMeshComponent::GetBoneQuatByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FQuat::Identity;

	// Quat getter도 Rotator getter와 같은 world-space 기준을 유지한다.
	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	const FMatrix BoneWorldMatrix = GlobalMatrices[BoneIndex] * GetWorldMatrix();
	return MatrixToEditorTransform(BoneWorldMatrix).Rotation;
}

FVector USkinnedMeshComponent::GetBoneScaleByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FVector::ZeroVector;

	// scale은 hierarchy와 component transform의 영향을 받은 최종 matrix에서 추출한다.
	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	const FMatrix BoneWorldMatrix = GlobalMatrices[BoneIndex] * GetWorldMatrix();
	return BoneWorldMatrix.GetScale();
}

FTransform USkinnedMeshComponent::GetBoneLocalTransformByIndex(int32 BoneIndex) const
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return FMatrix::Identity;

	// edit pose는 matrix로 보관하고, UI/API 표시 시점에만 transform으로 분해한다.
	if (bUseBoneEditPose && BoneEditLocalMatrices.size() == Asset->Bones.size())
	{
		return MatrixToEditorTransform(BoneEditLocalMatrices[BoneIndex]);
	}

	return Asset->Bones[BoneIndex].LocalMatrix;
}

void USkinnedMeshComponent::SetBoneLocationByIndex(int32 BoneIndex, const FVector& NewLocation)
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return;

	EnsureBoneEditPose();

	TArray<FMatrix> GlobalMatrices;
	BuildBoneEditGlobalMatrices(GlobalMatrices);

	// setter 입력은 world space이므로 component local global 위치로 변환한 뒤 parent local로 되돌린다.
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

void USkinnedMeshComponent::SetBoneRotationByIndex(int32 BoneIndex, const FRotator& NewRotation)
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

	// Matrix pose를 유지하면서 rotation 편집 지점에서만 editor transform으로 분해/재조립한다.
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

void USkinnedMeshComponent::SetBoneRotationByIndex(int32 BoneIndex, const FQuat& NewQuat)
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

	// world rotation을 component-local global rotation으로 바꾸고, parent inverse를 곱해 local pose에 저장한다.
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

void USkinnedMeshComponent::SetBoneScaleByIndex(int32 BoneIndex, const FVector& NewScale)
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return;

	EnsureBoneEditPose();

	// scale은 local transform 값 자체를 바꾸는 편집이므로 parent inverse 변환 없이 저장한다.
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

void USkinnedMeshComponent::SetBoneLocalTransformByIndex(int32 BoneIndex, const FTransform& NewLocalTransform)
{
	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset || BoneIndex < 0 || BoneIndex >= (int32)Asset->Bones.size()) return;

	EnsureBoneEditPose();
	// caller가 이미 local transform을 넘기는 API라서 hierarchy 변환 없이 override pose에 기록한다.
	BoneEditLocalMatrices[BoneIndex] = NewLocalTransform.ToMatrix();

	bUseBoneEditPose = true;
	UpdateCPUSkinning();
	MarkWorldBoundsDirty();
}

void USkinnedMeshComponent::GetCurrentBoneGlobalTransforms(TArray<FTransform>& OutGlobals) const
{
	BuildBoneEditGlobalTransforms(OutGlobals);
}

// Render buffer 섹션: SceneProxy가 index buffer와 section draw를 만들 때 asset render buffer만 필요로 한다.
FMeshBuffer* USkinnedMeshComponent::GetMeshBuffer() const
{
	// mesh가 없거나 resource 초기화 전이면 draw path가 안전하게 skip되도록 nullptr을 반환한다.
	if (!SkeletalMesh) return nullptr;
	FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
	if (!Asset || !Asset->RenderBuffer) return nullptr;
	return Asset->RenderBuffer.get();
}

FMeshDataView USkinnedMeshComponent::GetMeshDataView() const
{
	// static draw interface와 같은 모양의 view를 제공하지만, 실제 rendering은 SceneProxy의 skinned vertices를 쓴다.
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

// Skinning 섹션: asset bone hierarchy와 optional edit pose를 global transform 배열로 펼친다.
void USkinnedMeshComponent::BuildBoneEditGlobalMatrices(TArray<FMatrix>& OutGlobals) const
{
	OutGlobals.clear();

	FSkeletalMesh* Asset = SkeletalMesh ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset) return;

	const int32 BoneCount = static_cast<int32>(Asset->Bones.size());
	OutGlobals.resize(BoneCount);

	for (int32 i = 0; i < BoneCount; ++i)
	{
		// edit pose가 skeleton 크기와 맞을 때만 override를 사용해 stale cache를 방지한다.
		const FMatrix LocalMatrix = (bUseBoneEditPose && BoneEditLocalMatrices.size() == BoneCount)
			? BoneEditLocalMatrices[i] : Asset->Bones[i].LocalMatrix;

		// asset bone order가 parent-first라는 전제에 맞춰 부모 global을 누적한다.
		const int32 ParentIndex = Asset->Bones[i].ParentIndex;
		OutGlobals[i] = (ParentIndex >= 0) ? LocalMatrix * OutGlobals[ParentIndex] : LocalMatrix;
	}
}

// Cache 초기화는 resize까지만 담당하고, 실제 vertex 내용 갱신은 UpdateCPUSkinning에 모은다.
void USkinnedMeshComponent::InitSkinningCache()
{
	USkeletalMesh* Mesh = GetSkeletalMesh();
	if (!Mesh || !Mesh->GetSkeletalMeshAsset())
	{
		SkinnedVertices.clear();
		return;
	}

	FSkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
	SkinnedVertices.resize(Asset->Vertices.size());
}

// CPU skinning은 현재 renderer가 DynamicVertexBuffer에 올릴 FVertexPNCTT 배열을 만드는 단일 경로다.
void USkinnedMeshComponent::UpdateCPUSkinning()
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

	// FBX import 결과가 mesh range별 bind global을 가질 수 있어 range 단위 skinning을 유지한다.
	auto SkinVertexRange = [&](uint32 VertexStart, uint32 VertexEnd, const FMatrix& MeshBindGlobal)
		{
			TArray<FMatrix> SkinMatrices;
			SkinMatrices.resize(Asset->Bones.size(), FMatrix::Identity);

			// bind pose에서 현재 pose로 가는 행렬을 bone별로 미리 만들어 vertex loop 비용을 줄인다.
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

				// 현재 vertex format은 최대 4개 bone influence를 갖는다.
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
					// weight가 없는 vertex도 사라지지 않게 mesh bind transform만 적용한다.
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
					// tangent가 0이면 shader 입력 안정성을 위해 기본 축을 넣는다.
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
		// range 정보가 없는 구형 asset은 identity bind로 전체 vertex를 처리한다.
		SkinVertexRange(0, (uint32)Asset->Vertices.size(), FMatrix::Identity);
	}

	// SceneProxy는 revision 차이만 보고 dynamic vertex buffer upload 여부를 결정한다.
	++SkinnedRevision;
}

void USkinnedMeshComponent::BuildBoneEditGlobalTransforms(TArray<FTransform>& OutGlobals) const
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

// Material 섹션: material override 변경은 geometry 재생성 없이 proxy material만 dirty 처리한다.
void USkinnedMeshComponent::SetMaterial(int32 ElementIndex, UMaterial* InMaterial)
{
	if (ElementIndex < 0 || ElementIndex >= static_cast<int32>(OverrideMaterials.size()))
	{
		return;
	}

	OverrideMaterials[ElementIndex] = InMaterial;

	if (ElementIndex < static_cast<int32>(MaterialSlots.size()))
	{
		MaterialSlots[ElementIndex].Path = InMaterial
			? InMaterial->GetAssetPathFileName()
			: "None";
	}

	MarkProxyDirty(EDirtyFlag::Material);
}

UMaterial* USkinnedMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex >= 0 && ElementIndex < static_cast<int32>(OverrideMaterials.size()))
	{
		return OverrideMaterials[ElementIndex];
	}

	return nullptr;
}

// FArchive 기반 직렬화 — 복제 왕복용. 자산은 경로로만 들고, 실제 로드는 PostDuplicate에서.
static FArchive& operator<<(FArchive& Ar, FMaterialSlot& Slot)
{
	Ar << Slot.Path;
	return Ar;
}

void USkinnedMeshComponent::Serialize(FArchive& Ar)
{
	UMeshComponent::Serialize(Ar);
	// asset pointer는 session마다 달라질 수 있어 path와 slot path만 직렬화한다.
	Ar << SkeletalMeshPath;
	Ar << MaterialSlots;
}

// Duplicate/load 섹션: 저장된 path를 실제 asset pointer로 복원하되 dirty 처리는 SetSkeletalMesh에 위임한다.
void USkinnedMeshComponent::PostDuplicate()
{
	UMeshComponent::PostDuplicate();

	if (!SkeletalMeshPath.empty() && SkeletalMeshPath != "None")
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		USkeletalMesh* Loaded = FMeshManager::LoadSkeletalMesh(SkeletalMeshPath, Device);
		if (Loaded)
		{
			TArray<FMaterialSlot> SavedSlots = MaterialSlots;
			SetSkeletalMesh(Loaded);

			// SetSkeletalMesh가 default slot을 채운 뒤 저장된 override slot을 다시 덮어쓴다.
			for (int32 i = 0; i < (int32)MaterialSlots.size() && i < (int32)SavedSlots.size(); ++i)
			{
				MaterialSlots[i] = SavedSlots[i];

				const FString& MatPath = MaterialSlots[i].Path;
				if (MatPath.empty() || MatPath == "None")
				{
					SetMaterial(i, nullptr);
				}
				else
				{
					UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateMaterial(MatPath);
					SetMaterial(i, LoadedMat);
				}
			}
			
		}
	}
	else 
	{
		SetSkeletalMesh(nullptr);
	}
}

void USkinnedMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMeshComponent::GetEditableProperties(OutProps);
	// editor는 pointer 대신 path 문자열을 편집하고, PostEditProperty에서 load 흐름으로 진입한다.
	OutProps.push_back({ "Skeletal Mesh", EPropertyType::SkeletalMeshRef, "Mesh", &SkeletalMeshPath });
	for (int32 i = 0; i < static_cast<int32>(MaterialSlots.size()); ++i)
	{
		FPropertyDescriptor Desc;
		Desc.Name = "Element " + std::to_string(i);
		Desc.Type = EPropertyType::MaterialSlot;
		Desc.Category = "Materials";
		Desc.ValuePtr = &MaterialSlots[i];
		OutProps.push_back(Desc);
	}
}

void USkinnedMeshComponent::PostEditProperty(const char* PropertyName)
{
	UMeshComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Skeletal Mesh") == 0)
	{
		// mesh path 변경도 코드 경로와 같은 SetSkeletalMesh를 통과시켜 skinning과 dirty 처리를 통일한다.
		if (!SkeletalMeshPath.empty() && SkeletalMeshPath != "None")
		{
			ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
			USkeletalMesh* Loaded = FMeshManager::LoadSkeletalMesh(SkeletalMeshPath, Device);

			SetSkeletalMesh(Loaded);
		}
		else
		{
			SetSkeletalMesh(nullptr);
		}

	}

	if (strncmp(PropertyName, "Element ", 8) == 0)
	{
		// "Element 0"에서 8번째 인덱스부터 시작하는 숫자를 정수로 변환한다.
		int32 Index = atoi(&PropertyName[8]);

		// editor slot path 변경은 geometry와 무관하므로 SetMaterial의 material dirty만 사용한다.
		if (Index >= 0 && Index < (int32)MaterialSlots.size())
		{
			FString NewMatPath = MaterialSlots[Index].Path;

			if (NewMatPath == "None" || NewMatPath.empty())
			{
				SetMaterial(Index, nullptr);
			}
			else
			{
				UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateMaterial(NewMatPath);
				if (LoadedMat)
				{
					SetMaterial(Index, LoadedMat);
				}
			}
		}
	}
}

bool USkinnedMeshComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	if (!SkeletalMesh)
	{
		return false;
	}

	FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
	if (!Asset || Asset->Indices.empty() || SkinnedVertices.empty())
	{
		return false;
	}

	if (SkinnedVertices.size() != Asset->Vertices.size())
	{
		return false;
	}

	const FMatrix& WorldMatrix = GetWorldMatrix();
	const FMatrix& WorldInverse = GetWorldInverseMatrix();

	// SkinnedVertices 기반으로 Picking
	const bool bHit = FRayUtils::RaycastTriangles(
		Ray,
		WorldMatrix,
		WorldInverse,
		SkinnedVertices.data(),
		sizeof(FVertexPNCTT),
		Asset->Indices.data(),
		static_cast<uint32>(Asset->Indices.size()),
		OutHitResult);

	if (bHit)
	{
		OutHitResult.HitComponent = this;
	}

	return bHit;
}

void USkinnedMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// animation system이 없는 현재 구조에서는 edit pose 변경분을 CPU skinned vertices로 계속 반영한다.
	UpdateCPUSkinning();
}

void USkinnedMeshComponent::GetCurrentBoneGlobalMatrices(TArray<FMatrix>& OutGlobals) const
{
	BuildBoneEditGlobalMatrices(OutGlobals);
}
