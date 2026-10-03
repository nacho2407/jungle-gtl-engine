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
    WorldAABB.Reset();

    if (!HasValidMesh())
    {
        bBoundsDirty = false;
        return;
    }

    if (bEnableCPUSkinning)
    {
        const_cast<USkinnedMeshComponent*>(this)->EnsureSkinningUpdated();
    }

    const FMatrix& WorldMatrix = GetWorldMatrix();

    if (bEnableCPUSkinning && !SkinnedVertices.empty())
    {
        for (const FSkeletalMeshVertex& Vertex : SkinnedVertices)
        {
            WorldAABB.Expand(WorldMatrix.TransformPosition(Vertex.Position));
        }

        bBoundsDirty = false;
        return;
    }

    const FAABB& LocalBounds = SkeletalMesh->GetLocalBounds();
    if (LocalBounds.IsValid())
    {
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

        for (const FVector& Corner : LocalCorners)
        {
            WorldAABB.Expand(WorldMatrix.TransformPosition(Corner));
        }
    }

    bBoundsDirty = false;
}

bool USkinnedMeshComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
    if (!HasValidMesh())
    {
        return false;
    }

    // 현재 pose 기준 SkinnedVertices가 필요
    EnsureSkinningUpdated();

    EnsureBoundsUpdated();

    float BoxT = 0.0f;
    if (!WorldAABB.IntersectRay(Ray, BoxT))
    {
        return false;
    }

    const TArray<FSkeletalMeshVertex>& Vertices = SkinnedVertices;
    const TArray<uint32>& Indices = SkeletalMesh->GetIndices();

    if (Vertices.empty() || Indices.empty())
    {
        return false;
    }

    const FMatrix InvWorld = GetWorldMatrix().GetInverse();

    FRay LocalRay = Ray;
    LocalRay.Origin = InvWorld.TransformPosition(LocalRay.Origin);
    LocalRay.Direction = InvWorld.TransformVector(LocalRay.Direction);
    LocalRay.Direction.NormalizeSafe();

    bool bHit = false;
    float ClosestT = FLT_MAX;
    int32 BestFaceIndex = -1;
    FVector BestLocalNormal = FVector::ZeroVector;

    for (uint32 i = 0; i + 2 < static_cast<uint32>(Indices.size()); i += 3)
    {
        const uint32 I0 = Indices[i];
        const uint32 I1 = Indices[i + 1];
        const uint32 I2 = Indices[i + 2];

        if (I0 >= Vertices.size() || I1 >= Vertices.size() || I2 >= Vertices.size())
        {
            continue;
        }

        const FVector& V0 = Vertices[I0].Position;
        const FVector& V1 = Vertices[I1].Position;
        const FVector& V2 = Vertices[I2].Position;

        float HitT = 0.0f;
        if (IntersectTriangle(LocalRay.Origin, LocalRay.Direction, V0, V1, V2, HitT))
        {
            if (HitT < ClosestT)
            {
                ClosestT = HitT;
                bHit = true;
                BestFaceIndex = static_cast<int32>(i / 3);

                const FVector Edge1 = V1 - V0;
                const FVector Edge2 = V2 - V0;
                BestLocalNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();
            }
        }
    }

    if (!bHit)
    {
        return false;
    }

    const FVector LocalHitLocation = LocalRay.Origin + LocalRay.Direction * ClosestT;
    const FVector WorldHitLocation = GetWorldMatrix().TransformPosition(LocalHitLocation);

    FVector WorldNormal = GetWorldMatrix().TransformVector(BestLocalNormal);
    WorldNormal.NormalizeSafe();

    OutHitResult.bHit = true;
    OutHitResult.HitComponent = this;
    OutHitResult.Distance = (WorldHitLocation - Ray.Origin).Size();
    OutHitResult.Location = WorldHitLocation;
    OutHitResult.Normal = WorldNormal;
    OutHitResult.FaceIndex = BestFaceIndex;

    return true;
}

const FAABB& USkinnedMeshComponent::GetWorldAABB() const
{
    EnsureBoundsUpdated();
    return WorldAABB;
}

bool USkinnedMeshComponent::ConsumeRenderStateDirty()
{
    const bool bWasDirty = bRenderStateDirty;
    bRenderStateDirty = false;
    return bWasDirty;
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

    if (!HasValidMesh())
    {
        return;
    }

    UpdateCurrentGlobalPose();
    UpdateSkinningMatrices();
    SkinVerticesCPU();

    bSkinningDirty = false;
    MarkBoundsDirty();
    MarkRenderStateDirty();

    // Bone 자세가 새로 계산됐으므로, *내* socket으로 붙은 child의 world transform이 stale.
    // FName::None도 IsValid이므로 명시적 != 비교 + HasSocket 둘 다 확인.
    for (USceneComponent* Child : ChildComponents)
    {
        if (!Child) continue;
        const FName& SocketName = Child->GetAttachSocketName();
        if (SocketName != FName::None && HasSocket(SocketName))
        {
            Child->MarkTransformDirty();   // 자식 손주까지 재귀 dirty 전파됨
        }
    }
}

bool USkinnedMeshComponent::HasSocket(const FName& SocketName) const
{
    return SkeletalMesh ? SkeletalMesh->HasSocket(SocketName) : false;
}

FMatrix USkinnedMeshComponent::GetBoneWorldMatrix(int32 BoneIndex) const
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(CurrentGlobalPose.size()))
    {
        return GetWorldMatrix();
    }
    // row-vector 규약: BoneWorld = BoneGlobal · ActorWorld (GetSocketTransform과 동일)
    return CurrentGlobalPose[BoneIndex] * GetWorldMatrix();
}

FTransform USkinnedMeshComponent::GetSocketTransform(const FName& SocketName) const
{
    if (!SkeletalMesh)
    {
        return GetWorldTransform();
    }

    const FSkeletalMeshSocket* Socket = SkeletalMesh->FindSocket(SocketName);
    if (!Socket)
    {
        return GetWorldTransform();
    }

    if (Socket->BoneIndex < 0 || Socket->BoneIndex >= static_cast<int32>(CurrentGlobalPose.size()))
    {
        return GetWorldTransform();
    }

    // row-vector 규약: v · SocketRel · BoneGlobal · ActorWorld
    //   SocketRel    : 본 local 기준 socket offset
    //   BoneGlobal   : 현재 본 자세 (fbx-world 공간, EnsureSkinningUpdated가 미리 갱신)
    //   ActorWorld   : 액터의 game-world 변환
    const FMatrix SocketRel  = Socket->GetRelativeTransform();
    const FMatrix BoneGlobal = CurrentGlobalPose[Socket->BoneIndex];
    const FMatrix ActorWorld = GetWorldMatrix();
    const FMatrix Combined   = SocketRel * BoneGlobal * ActorWorld;
    return FTransform(Combined);
}

void USkinnedMeshComponent::InitializePoseFromBindPose()
{
    CurrentLocalPose.clear();
    CurrentGlobalPose.clear();
    SkinningMatrices.clear();

    if (!HasValidMesh())
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
    if (!HasValidMesh())
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

    if (!HasValidMesh())
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
    if (!HasValidMesh())
    {
        SkinnedVertices.clear();
        return;
    }

	// 아직 변형되지 않은 bind pose 기준 정점 목록
	// index buffer는 당연히 재사용!
    const TArray<FSkeletalMeshVertex>& SourceVertices = SkeletalMesh->GetVertices();
    const TArray<FBoneInfo>& Bones = SkeletalMesh->GetBones();
    const int32 BoneCount = static_cast<int32>(Bones.size());

    SkinnedVertices.resize(SourceVertices.size());

    for (int32 VertexIndex = 0; VertexIndex < static_cast<int32>(SourceVertices.size()); ++VertexIndex)
    {
        const FSkeletalMeshVertex& Src = SourceVertices[VertexIndex];

		// 유효한 weight들의 합 구하기
		// vertex는 최대 4개의 bone influence를 가질 수 있음을 가정
        float ValidWeightSum = 0.0f;
        for (int32 InfluenceIndex = 0; InfluenceIndex < 4; ++InfluenceIndex)
        {
            const int32 BoneIndex = Src.BoneIndices[InfluenceIndex];
            const float Weight = Src.BoneWeights[InfluenceIndex];

            if (BoneIndex < BoneCount && Weight > 0.0f)
            {
                ValidWeightSum += Weight;
            }
        }

		// weight가 하나도 없으면 원본 그대로 복사(아주 작은 오차값 허용)
        if (ValidWeightSum <= 1e-6f)
        {
            SkinnedVertices[VertexIndex] = Src;
            continue;
        }

		// 누적을 위한 변수들 초기화
        FVector SkinnedPosition = FVector::ZeroVector;
        FVector SkinnedNormal = FVector::ZeroVector;
        FVector SkinnedTangent = FVector::ZeroVector;
        const FVector SourceTangent(Src.Tangent.X, Src.Tangent.Y, Src.Tangent.Z);

		// 실제 skinning 계산 루프
        for (int32 InfluenceIndex = 0; InfluenceIndex < 4; ++InfluenceIndex)
        {
            const int32 BoneIndex = Src.BoneIndices[InfluenceIndex];
            const float RawWeight = Src.BoneWeights[InfluenceIndex];

            if (BoneIndex >= BoneCount || RawWeight <= 0.0f)
            {
                continue;
            }

			// bone weight의 합은 보통 1이어야 하므로 여기서 한 번 정규화
            const float Weight = RawWeight / ValidWeightSum;
            const FMatrix& SkinMatrix = SkinningMatrices[BoneIndex];

            SkinnedPosition += SkinMatrix.TransformPosition(Src.Position) * Weight;
            SkinnedNormal += SkinMatrix.TransformVector(Src.Normal) * Weight;
            SkinnedTangent += SkinMatrix.TransformVector(SourceTangent) * Weight;
        }

		// 여러 bone들을 섞으면 쿠크다스 normal이 또 깨질 수 있으므로 여기서 다시 normalize
		// fallback은 영벡터 대신 원래 normal / tangent
        if (!SkinnedNormal.NormalizeSafe())
        {
            SkinnedNormal = Src.Normal;
            SkinnedNormal.NormalizeSafe();
        }

        if (!SkinnedTangent.NormalizeSafe())
        {
            SkinnedTangent = SourceTangent;
            SkinnedTangent.NormalizeSafe();
        }

        FSkeletalMeshVertex& Dst = SkinnedVertices[VertexIndex];
        Dst = Src;
        Dst.Position = SkinnedPosition;
        Dst.Normal = SkinnedNormal;
        Dst.Tangent = FVector4(SkinnedTangent.X, SkinnedTangent.Y, SkinnedTangent.Z, Src.Tangent.W);
    }
}

void USkinnedMeshComponent::EnsureBoundsUpdated() const
{
    if (!bBoundsDirty && !bTransformDirty)
    {
        return;
    }

    const_cast<USkinnedMeshComponent*>(this)->UpdateWorldAABB();
}
