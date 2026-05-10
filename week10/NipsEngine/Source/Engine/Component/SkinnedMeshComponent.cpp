#include "SkinnedMeshComponent.h"

#include "Core/ResourceManager.h"
#include "Render/Resource/Material.h"

#include <algorithm>
#include <cfloat>
#include <cstring>

DEFINE_CLASS(USkinnedMeshComponent, UMeshComponent)

namespace
{
static void ComputeGlobalPoseRecursive(
    int32 BoneIndex,
    const TArray<FBoneInfo>& Bones,
    const TArray<FMatrix>& LocalPose,
    TArray<FMatrix>& GlobalPose,
    TArray<bool>& Visited)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Bones.size()))
    {
        return;
    }

    if (Visited[BoneIndex])
    {
        return;
    }

	/*
	 * note: FBX에서 bone 배열 순서가 반드시 parent가 child보다 앞에 온다고 보장하기 어려움.
	 *       부모 쪽이 먼저 계산되어야 하기 때문에 부모 방향으로 재귀를 하되, 이미 계산된 부분은
	 *       넘어가기 위해서 Visited 추가
	 */
    const int32 ParentIndex = Bones[BoneIndex].ParentIndex;
    if (ParentIndex >= 0)
    {
        ComputeGlobalPoseRecursive(ParentIndex, Bones, LocalPose, GlobalPose, Visited);
        GlobalPose[BoneIndex] = LocalPose[BoneIndex] * GlobalPose[ParentIndex];
    }
    else
    {
        GlobalPose[BoneIndex] = LocalPose[BoneIndex];
    }

    Visited[BoneIndex] = true;
}
} // namespace

void USkinnedMeshComponent::Serialize(FArchive& Ar)
{
    if (Ar.IsLoading())
    {
        Ar << "SkeletalMeshAsset" << SkeletalMeshPath;
        if (!SkeletalMeshPath.empty())
        {
            SetSkeletalMesh(FResourceManager::Get().LoadSkeletalMesh(SkeletalMeshPath));
        }
        UMeshComponent::Serialize(Ar);
        return;
    }

    UMeshComponent::Serialize(Ar);
    Ar << "SkeletalMeshAsset" << SkeletalMeshPath;
}

void USkinnedMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UMeshComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "SkeletalMesh", EPropertyType::String, &SkeletalMeshPath });
    OutProps.push_back({ "Materials", EPropertyType::Material, &Materials });
}

void USkinnedMeshComponent::PostEditProperty(const char* PropertyName)
{
    UMeshComponent::PostEditProperty(PropertyName);

    if (std::strcmp(PropertyName, "SkeletalMesh") == 0)
    {
        if (SkeletalMeshPath.empty())
        {
            SetSkeletalMesh(nullptr);
            return;
        }

        SetSkeletalMesh(FResourceManager::Get().LoadSkeletalMesh(SkeletalMeshPath));
    }
    else if (std::strcmp(PropertyName, "Materials") == 0)
    {
        for (int32 i = 0; i < static_cast<int32>(Materials.size()); ++i)
        {
            SetMaterial(i, Materials[i]);
        }

        MarkRenderStateDirty();
    }
}

void USkinnedMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
    if (SkeletalMesh == InSkeletalMesh)
    {
        return;
    }

    SkeletalMesh = InSkeletalMesh;
    Materials.clear();
    CurrentLocalPose.clear();
    CurrentGlobalPose.clear();
    SkinningMatrices.clear();
    SkinnedVertices.clear();

    if (SkeletalMesh)
    {
        SkeletalMeshPath = SkeletalMesh->GetAssetPathFileName();

        const TArray<FStaticMeshSection>& Sections = SkeletalMesh->GetSections();
        const TArray<FStaticMeshMaterialSlot>& Slots = SkeletalMesh->GetMaterialSlots();

        Materials.reserve(Sections.size());
        for (int32 SectionIndex = 0; SectionIndex < static_cast<int32>(Sections.size()); SectionIndex++)
        {
            const int32 SlotIndex = Sections[SectionIndex].MaterialSlotIndex;
            if (SlotIndex >= 0 && SlotIndex < static_cast<int32>(Slots.size()))
            {
                Materials.push_back(Slots[SlotIndex].Material);
            }
            else
            {
                Materials.push_back(nullptr);
            }
        }

        InitializePoseFromBindPose();
        SkinnedVertices.resize(SkeletalMesh->GetVertices().size());
    }
    else
    {
        SkeletalMeshPath.clear();
    }

    MarkBoundsDirty();
    MarkRenderStateDirty();
    MarkSkinningDirty();
}

void USkinnedMeshComponent::UpdateWorldAABB() const
{
	// TODO: 일단 현재는 reference pose 기준으로 bound 계산중... 이 부분은 나중에 팀 정책이 확정되면 변경

    WorldAABB.Reset();

    if (!SkeletalMesh || !SkeletalMesh->HasValidMeshData())
    {
        bBoundsDirty = false;
        return;
    }

    const FAABB& LocalBounds = SkeletalMesh->GetLocalBounds();
    if (!LocalBounds.IsValid())
    {
        bBoundsDirty = false;
        return;
    }

    const FVector LocalCorners[8] = {
        FVector(LocalBounds.Min.X, LocalBounds.Min.Y, LocalBounds.Min.Z),
        FVector(LocalBounds.Max.X, LocalBounds.Min.Y, LocalBounds.Min.Z),
        FVector(LocalBounds.Min.X, LocalBounds.Max.Y, LocalBounds.Min.Z),
        FVector(LocalBounds.Max.X, LocalBounds.Max.Y, LocalBounds.Min.Z),
        FVector(LocalBounds.Min.X, LocalBounds.Min.Y, LocalBounds.Max.Z),
        FVector(LocalBounds.Max.X, LocalBounds.Min.Y, LocalBounds.Max.Z),
        FVector(LocalBounds.Min.X, LocalBounds.Max.Y, LocalBounds.Max.Z),
        FVector(LocalBounds.Max.X, LocalBounds.Max.Y, LocalBounds.Max.Z)
    };

    const FMatrix& WorldMatrix = GetWorldMatrix();

    for (const FVector& Corner : LocalCorners)
    {
        const FVector WorldPos = WorldMatrix.TransformPosition(Corner);
        WorldAABB.Expand(WorldPos);
    }

    bBoundsDirty = false;
}

bool USkinnedMeshComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	// TODO: Skeletal Mesh의 Raycast 구현 필요...
    (void)Ray;
    (void)OutHitResult;
    return false;
}

void USkinnedMeshComponent::EnsureSkinningUpdated()
{
    if (!bEnableCPUSkinning)
    {
        return;
    }

    if (!bSkinningDirty)
    {
        return;
    }

    if (!SkeletalMesh || !SkeletalMesh->HasValidMeshData())
    {
        return;
    }

    UpdateCurrentGlobalPose();
    UpdateSkinningMatrices();
    SkinVerticesCPU();

    bSkinningDirty = false;
    MarkBoundsDirty();
    MarkRenderStateDirty();
}

void USkinnedMeshComponent::InitializePoseFromBindPose()
{
    CurrentLocalPose.clear();
    CurrentGlobalPose.clear();
    SkinningMatrices.clear();

    if (!SkeletalMesh || !SkeletalMesh->HasValidMeshData())
    {
        return;
    }

    const TArray<FBoneInfo>& Bones = SkeletalMesh->GetBones();
    const int32 BoneCount = static_cast<int32>(Bones.size());

    CurrentLocalPose.resize(BoneCount);
    CurrentGlobalPose.resize(BoneCount);
    SkinningMatrices.resize(BoneCount);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; BoneIndex++)
    {
        CurrentLocalPose[BoneIndex] = Bones[BoneIndex].LocalBindTransform;
        CurrentGlobalPose[BoneIndex] = Bones[BoneIndex].GlobalBindTransform;
        SkinningMatrices[BoneIndex] = FMatrix::Identity; // 계산은 초기화 함수에서 하지 않고 별도의 업데이트 함수에서 진행
    }
}

void USkinnedMeshComponent::UpdateCurrentGlobalPose()
{
    if (!SkeletalMesh || !SkeletalMesh->HasValidMeshData())
    {
        return;
    }

    const TArray<FBoneInfo>& Bones = SkeletalMesh->GetBones();
    const int32 BoneCount = static_cast<int32>(Bones.size());

    if (CurrentLocalPose.size() != Bones.size())
    {
        InitializePoseFromBindPose();
    }

    CurrentGlobalPose.resize(BoneCount);

    TArray<bool> Visited;
    Visited.resize(BoneCount, false);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        ComputeGlobalPoseRecursive(BoneIndex, Bones, CurrentLocalPose, CurrentGlobalPose, Visited);
    }
}

void USkinnedMeshComponent::UpdateSkinningMatrices()
{
    /*
	 * note: CurrentGlobalPose가 이 함수 호출 전에 제대로 구성되어 있어야함에 유의!
	 */

    if (!SkeletalMesh || !SkeletalMesh->HasValidMeshData())
    {
        return;
    }

    const TArray<FBoneInfo>& Bones = SkeletalMesh->GetBones();
    const int32 BoneCount = static_cast<int32>(Bones.size());

    SkinningMatrices.resize(BoneCount);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
		// row-vector convention 고려
		// reference pose라면 계산 결과가 identity여야 함
        SkinningMatrices[BoneIndex] = Bones[BoneIndex].InverseBindPose * CurrentGlobalPose[BoneIndex];
    }
}

void USkinnedMeshComponent::SkinVerticesCPU()
{
    if (!SkeletalMesh || !SkeletalMesh->HasValidMeshData())
    {
        SkinnedVertices.clear();
        return;
    }

	// 아직 변형되지 않은 bind pose 기준 정점 목록
	// index buffer는 당연히 재사용!
    const TArray<FSkeletalMeshVertex>& SourceVertices = SkeletalMesh->GetVertices();
    const TArray<FBoneInfo>& Bones = SkeletalMesh->GetBones();

    SkinnedVertices.resize(SourceVertices.size());

    for (int32 VertexIndex = 0; VertexIndex < static_cast<int32>(SourceVertices.size()); ++VertexIndex)
    {
        const FSkeletalMeshVertex& Src = SourceVertices[VertexIndex];

		// 유효한 weight들의 합 구하기
		// vertex는 최대 4개의 bone influence를 가질 수 있음을 가정
        float ValidWeightSum = 0.0f;
        for (int32 InfluenceIndex = 0; InfluenceIndex < 4; ++InfluenceIndex)
        {
			// TODO: BoneIndices는 아직 uint8임. 나중에 int32로 바뀔 것을 고려
            const int32 BoneIndex = Src.BoneIndices[InfluenceIndex];
            const float Weight = Src.BoneWeights[InfluenceIndex];

            if (BoneIndex >= 0 && BoneIndex < static_cast<int32>(Bones.size()) && Weight > 0.0f)
            {
                ValidWeightSum += Weight;
            }
        }

		// weight가 하나도 없으면 원본 그대로 복사(아주 작은 오차값 허용)
        if (ValidWeightSum <= 1e-6f)
        {
            SkinnedVertices[VertexIndex] = ConvertToNormalVertexWithoutSkinning(Src);
            continue;
        }

		// 누적을 위한 변수들 초기화
        FVector SkinnedPosition = FVector::ZeroVector;
        FVector SkinnedNormal = FVector::ZeroVector;
        FVector SkinnedTangent = FVector::ZeroVector;

		// 실제 skinning 계산 루프
        for (int32 InfluenceIndex = 0; InfluenceIndex < 4; ++InfluenceIndex)
        {
            // TODO: 여기도 마찬가지로 BoneIndices는 아직 uint8이라는 것을 고려할 것. 나중에 int32로 바뀔 것을 고려
            const int32 BoneIndex = Src.BoneIndices[InfluenceIndex];
            const float RawWeight = Src.BoneWeights[InfluenceIndex];

            if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Bones.size()) || RawWeight <= 0.0f)
            {
                continue;
            }

			// bone weight의 합은 보통 1이어야 하므로 여기서 한 번 정규화
            const float Weight = RawWeight / ValidWeightSum;
            const FMatrix& SkinMatrix = SkinningMatrices[BoneIndex];

            SkinnedPosition += SkinMatrix.TransformPosition(Src.Position) * Weight;
            SkinnedNormal += SkinMatrix.TransformVector(Src.Normal) * Weight;

            const FVector SourceTangent(Src.Tangent.X, Src.Tangent.Y, Src.Tangent.Z);
            SkinnedTangent += SkinMatrix.TransformVector(SourceTangent) * Weight;
        }

		// 여러 bone들을 섞으면 쿠크다스 normal이 또 깨질 수 있으므로 여기서 다시 normalize
        SkinnedNormal.NormalizeSafe();
        SkinnedTangent.NormalizeSafe();

        SkinnedVertices[VertexIndex].Position = SkinnedPosition;
        SkinnedVertices[VertexIndex].Color = Src.Color;
        SkinnedVertices[VertexIndex].Normal = SkinnedNormal;
        SkinnedVertices[VertexIndex].UVs = Src.UVs;
        SkinnedVertices[VertexIndex].Tangent = FVector4(SkinnedTangent.X, SkinnedTangent.Y, SkinnedTangent.Z, Src.Tangent.W);
    }
}

FNormalVertex USkinnedMeshComponent::ConvertToNormalVertexWithoutSkinning(const FSkeletalMeshVertex& Source) const
{
    FNormalVertex Out = {};
    Out.Position = Source.Position;
    Out.Color = Source.Color;
    Out.Normal = Source.Normal;
    Out.UVs = Source.UVs;
    Out.Tangent = Source.Tangent;
    return Out;
}
