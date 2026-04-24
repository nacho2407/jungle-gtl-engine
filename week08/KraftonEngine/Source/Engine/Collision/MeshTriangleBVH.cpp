// 충돌/피킹 영역의 세부 동작을 구현합니다.
#include "Collision/MeshTriangleBVH.h"

#include "Collision/RayUtils.h"
#include "Collision/RayUtilsSIMD.h"
#include "Mesh/StaticMeshAsset.h"
#include "Core/EngineTypes.h"

#include <algorithm>
#include <bit>
#include <cfloat>

namespace
{
// 메시 내부 BVH는 world BVH보다 leaf가 작고 traversal stack도 더 얕게 유지합니다.
constexpr int32 MeshBVHChildFanout = 8;
constexpr int32 MeshBVHMaxTraversalStack = 256;

// 삼각형 한 개를 감싸는 local-space AABB를 만듭니다.
FBoundingBox MakeTriangleBounds(const FVector& V0, const FVector& V1, const FVector& V2)
{
    FBoundingBox Bounds;
    Bounds.Expand(V0);
    Bounds.Expand(V1);
    Bounds.Expand(V2);
    return Bounds;
}

float GetAxisComponent(const FVector& Vector, int32 Axis)
{
    return Axis == 0 ? Vector.X : (Axis == 1 ? Vector.Y : Vector.Z);
}

// bucket 기반 분할 비용을 비교하기 위해 각 bucket bounds의 표면적을 사용합니다.
float GetBoundsSurfaceArea(const FBoundingBox& Bounds)
{
    const FVector Extent = Bounds.GetExtent();
    const float Width = std::max(Extent.X * 2.0f, 0.0f);
    const float Height = std::max(Extent.Y * 2.0f, 0.0f);
    const float Depth = std::max(Extent.Z * 2.0f, 0.0f);
    return 2.0f * ((Width * Height) + (Width * Depth) + (Height * Depth));
}

} // namespace

void FMeshTriangleBVH::BuildNow(const FStaticMesh& Mesh)
{
    // 메시가 바뀌었을 때 triangle leaf와 packet, node 배열을 통째로 다시 만듭니다.
    TriangleLeaves.clear();
    LeafPackets.clear();
    Nodes.clear();

    if (Mesh.Vertices.empty() || Mesh.Indices.size() < 3)
    {
        return;
    }

    TriangleLeaves.reserve(Mesh.Indices.size() / 3);
    LeafPackets.reserve((Mesh.Indices.size() / 3 + 7) / 8);

    // 인덱스 버퍼를 삼각형 단위로 훑으면서 각 삼각형의 bounds와 시작 인덱스를 leaf로 만듭니다.
    for (size_t Index = 0; Index + 2 < Mesh.Indices.size(); Index += 3)
    {
        const FVector& V0 = Mesh.Vertices[Mesh.Indices[Index]].Position;
        const FVector& V1 = Mesh.Vertices[Mesh.Indices[Index + 1]].Position;
        const FVector& V2 = Mesh.Vertices[Mesh.Indices[Index + 2]].Position;

        FTriangleLeaf Leaf;
        Leaf.TriangleStartIndex = static_cast<int32>(Index);
        Leaf.Bounds = MakeTriangleBounds(V0, V1, V2);
        TriangleLeaves.push_back(Leaf);
    }

    if (!TriangleLeaves.empty())
    {
        BuildRecursive(Mesh, 0, static_cast<int32>(TriangleLeaves.size()));
    }
}

void FMeshTriangleBVH::EnsureBuilt(const FStaticMesh& Mesh)
{
    // static mesh asset은 로드 후 고정된다고 보고, 아직 비어 있을 때만 1회 빌드합니다.
    if (!Nodes.empty())
    {
        return;
    }
    BuildNow(Mesh);
}

bool FMeshTriangleBVH::RaycastLocal(const FVector& LocalOrigin, const FVector& LocalDirection, const FStaticMesh& Mesh, FHitResult& OutHitResult) const
{
    // 로컬 공간 ray로 메시 BVH를 front-to-back 순회하면서 가장 가까운 삼각형 hit를 찾습니다
    struct FTraversalEntry
    {
        int32 NodeIndex;
        float TMin;
    };

    OutHitResult = {};
    if (Nodes.empty() || Mesh.Vertices.empty() || Mesh.Indices.size() < 3)
    {
        return false;
    }

    FTraversalEntry NodeStack[MeshBVHMaxTraversalStack];
    int32 StackSize = 0;

    // world 단계에서 이미 world transform을 벗겼으므로 여기서는 local ray만 다루면 됩니다.
    FRay LocalRay;
    LocalRay.Origin = LocalOrigin;
    LocalRay.Direction = LocalDirection;

    float RootTMin = 0.0f;
    float RootTMax = 0.0f;
    if (!FRayUtils::IntersectRayAABB(LocalRay, Nodes[0].Bounds.Min, Nodes[0].Bounds.Max, RootTMin, RootTMax))
    {
        return false;
    }

    const FRaySIMDContext RayContext = FRayUtilsSIMD::MakeRayContext(LocalOrigin, LocalDirection);

    // 재귀 대신 고정 크기 스택으로 순회해 call overhead와 allocation을 피합니다.
    NodeStack[StackSize++] = { 0, RootTMin };

    float ClosestT = FLT_MAX;
    bool bHit = false;

    while (StackSize > 0)
    {
        const FTraversalEntry Entry = NodeStack[--StackSize];
        if (Entry.TMin > ClosestT)
        {
            continue;
        }

        const FNode& Node = Nodes[Entry.NodeIndex];

        if (Node.IsLeaf())
        {
            // leaf는 최대 8개 triangle을 SoA packet으로 묶어 두었기 때문에
            // raycast 시에는 triangle 교차를 packet 단위 SIMD 한 번으로 처리할 수 있습니다.
            const FTrianglePacket& Packet = LeafPackets[Node.PacketIndex];

            alignas(32) float TValues[8];
            const int32 Mask = FRayUtilsSIMD::IntersectTriangles8Precomputed(
                                   RayContext,
                                   Packet.V0X, Packet.V0Y, Packet.V0Z,
                                   Packet.Edge1X, Packet.Edge1Y, Packet.Edge1Z,
                                   Packet.Edge2X, Packet.Edge2Y, Packet.Edge2Z,
                                   ClosestT,
                                   TValues) &
                               static_cast<int32>(Packet.LaneMask);

            if (Mask != 0)
            {
                // hit lane만 비트 스캔으로 순회해 scalar 후처리 비용을 줄인다.
                uint32 RemainingMask = static_cast<uint32>(Mask);
                while (RemainingMask != 0)
                {
                    const uint32 Lane = std::countr_zero(RemainingMask);
                    if (TValues[Lane] < ClosestT)
                    {
                        ClosestT = TValues[Lane];
                        OutHitResult.bHit = true;
                        OutHitResult.Distance = TValues[Lane];
                        OutHitResult.FaceIndex = Packet.TriangleStartIndices[Lane];
                        bHit = true;
                    }
                    RemainingMask &= (RemainingMask - 1);
                }
            }
            continue;
        }

        // internal node에서는 child AABB 8개를 한 번에 검사한 뒤,
        // 실제로 맞은 child만 추려 가까운 순서대로 방문합니다.
        alignas(32) float TMinValues[8];
        const int32 NodeMask = FRayUtilsSIMD::IntersectAABB8(
            RayContext,
            Node.ChildMinX, Node.ChildMinY, Node.ChildMinZ,
            Node.ChildMaxX, Node.ChildMaxY, Node.ChildMaxZ,
            ClosestT,
            TMinValues);
        if (NodeMask == 0)
        {
            continue;
        }

        FTraversalEntry ChildEntries[8];
        int32 ChildEntryCount = 0;
        // 자식 수는 최대 8개라서 hit child만 모아도 정렬 비용이 작고,
        // 가까운 순서대로 방문하면 ClosestT가 빨리 줄어 후속 prune이 잘 됩니다
        uint32 RemainingMask = static_cast<uint32>(NodeMask) & ((1u << Node.ChildCount) - 1u);
        while (RemainingMask != 0)
        {
            const uint32 Lane = std::countr_zero(RemainingMask);
            ChildEntries[ChildEntryCount++] = { Node.Children[Lane], TMinValues[Lane] };
            RemainingMask &= (RemainingMask - 1);
        }

        if (ChildEntryCount == 1)
        {
            if (StackSize < MeshBVHMaxTraversalStack)
            {
                NodeStack[StackSize++] = ChildEntries[0];
            }
            continue;
        }

        if (ChildEntryCount == 2 && ChildEntries[0].TMin < ChildEntries[1].TMin)
        {
            std::swap(ChildEntries[0], ChildEntries[1]);
        }
        else if (ChildEntryCount > 2)
        {
            for (int32 I = 1; I < ChildEntryCount; ++I)
            {
                FTraversalEntry Key = ChildEntries[I];
                int32 J = I - 1;
                while (J >= 0 && ChildEntries[J].TMin < Key.TMin)
                {
                    ChildEntries[J + 1] = ChildEntries[J];
                    --J;
                }
                ChildEntries[J + 1] = Key;
            }
        }

        for (int32 I = 0; I < ChildEntryCount && StackSize < MeshBVHMaxTraversalStack; ++I)
        {
            NodeStack[StackSize++] = ChildEntries[I];
        }
    }

    return bHit;
}

int32 FMeshTriangleBVH::BuildRecursive(const FStaticMesh& Mesh, int32 Start, int32 End)
{
    // triangle leaf 구간 [Start, End)를 하나의 node로 만들고, 필요하면 재귀 분할합니다.
    const int32 NodeIndex = static_cast<int32>(Nodes.size());
    Nodes.emplace_back();

    FBoundingBox Bounds;
    for (int32 LeafIndex = Start; LeafIndex < End; ++LeafIndex)
    {
        Bounds.Expand(TriangleLeaves[LeafIndex].Bounds.Min);
        Bounds.Expand(TriangleLeaves[LeafIndex].Bounds.Max);
    }
    Nodes[NodeIndex].Bounds = Bounds;

    constexpr int32 MaxLeafSize = 8;
    const int32 LeafCount = End - Start;
    if (LeafCount <= MaxLeafSize)
    {
        // leaf node는 삼각형들을 packet 하나로 압축해 raycast 시 재계산 없이 바로 SIMD 검사합니다.
        Nodes[NodeIndex].FirstLeaf = Start;
        Nodes[NodeIndex].LeafCount = LeafCount;
        Nodes[NodeIndex].PacketIndex = static_cast<int32>(LeafPackets.size());

        FTrianglePacket Packet{};
        Packet.TriangleCount = static_cast<uint32>(LeafCount);

        for (int32 i = 0; i < 8; ++i)
        {
            if (i < LeafCount)
            {
                const int32 TriStartIndex = TriangleLeaves[Start + i].TriangleStartIndex;
                const FVector& V0 = Mesh.Vertices[Mesh.Indices[TriStartIndex]].Position;
                const FVector& V1 = Mesh.Vertices[Mesh.Indices[TriStartIndex + 1]].Position;
                const FVector& V2 = Mesh.Vertices[Mesh.Indices[TriStartIndex + 2]].Position;
                const FVector Edge1 = V1 - V0;
                const FVector Edge2 = V2 - V0;

                Packet.V0X[i] = V0.X;
                Packet.V0Y[i] = V0.Y;
                Packet.V0Z[i] = V0.Z;
                Packet.Edge1X[i] = Edge1.X;
                Packet.Edge1Y[i] = Edge1.Y;
                Packet.Edge1Z[i] = Edge1.Z;
                Packet.Edge2X[i] = Edge2.X;
                Packet.Edge2Y[i] = Edge2.Y;
                Packet.Edge2Z[i] = Edge2.Z;
                Packet.TriangleStartIndices[i] = TriStartIndex;
                Packet.LaneMask |= (1u << i);
            }
            else
            {
                Packet.V0X[i] = Packet.V0Y[i] = Packet.V0Z[i] = 0.0f;
                Packet.Edge1X[i] = Packet.Edge1Y[i] = Packet.Edge1Z[i] = 0.0f;
                Packet.Edge2X[i] = Packet.Edge2Y[i] = Packet.Edge2Z[i] = 0.0f;
            }
        }

        LeafPackets.push_back(Packet);
        return NodeIndex;
    }

    float BestCost = FLT_MAX;
    int32 BestAxis = 0;
    bool bFoundValidAxis = false;

    // 단순 longest-axis 대신, 각 축을 8개 bucket으로 나눠
    // surface area * primitive count 비용이 가장 작은 축을 고릅니다.
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        float MinCenter = FLT_MAX;
        float MaxCenter = -FLT_MAX;

        for (int32 LeafIndex = Start; LeafIndex < End; ++LeafIndex)
        {
            const float Center = GetAxisComponent(TriangleLeaves[LeafIndex].Bounds.GetCenter(), Axis);
            MinCenter = std::min(MinCenter, Center);
            MaxCenter = std::max(MaxCenter, Center);
        }

        if (MaxCenter - MinCenter <= 1e-4f)
        {
            continue;
        }

        FBoundingBox BucketBounds[MeshBVHChildFanout];
        int32 BucketCounts[MeshBVHChildFanout] = {};
        const float Scale = static_cast<float>(MeshBVHChildFanout) / (MaxCenter - MinCenter);

        for (int32 LeafIndex = Start; LeafIndex < End; ++LeafIndex)
        {
            const FBoundingBox& LeafBounds = TriangleLeaves[LeafIndex].Bounds;
            const float Center = GetAxisComponent(LeafBounds.GetCenter(), Axis);
            int32 Bucket = static_cast<int32>((Center - MinCenter) * Scale);
            Bucket = std::clamp(Bucket, 0, MeshBVHChildFanout - 1);
            BucketBounds[Bucket].Expand(LeafBounds.Min);
            BucketBounds[Bucket].Expand(LeafBounds.Max);
            BucketCounts[Bucket]++;
        }

        float AxisCost = 0.0f;
        for (int32 Bucket = 0; Bucket < MeshBVHChildFanout; ++Bucket)
        {
            if (BucketCounts[Bucket] == 0)
            {
                continue;
            }
            AxisCost += GetBoundsSurfaceArea(BucketBounds[Bucket]) * static_cast<float>(BucketCounts[Bucket]);
        }

        if (AxisCost < BestCost)
        {
            BestCost = AxisCost;
            BestAxis = Axis;
            bFoundValidAxis = true;
        }
    }

    if (!bFoundValidAxis)
    {
        // centroid 분포가 거의 한 점으로 겹치면 bucket 비용 비교가 무의미하므로
        // longest-axis 기반 정렬 후 균등 분할로 되돌립니다.
        const FBoundingBox& ReferenceBounds = TriangleLeaves[Start].Bounds;
        const FVector Extent = ReferenceBounds.GetExtent();
        BestAxis = (Extent.Y > Extent.X && Extent.Y >= Extent.Z) ? 1 : ((Extent.Z > Extent.X && Extent.Z > Extent.Y) ? 2 : 0);

        std::sort(
            TriangleLeaves.begin() + Start,
            TriangleLeaves.begin() + End,
            [BestAxis](const FTriangleLeaf& A, const FTriangleLeaf& B)
            {
                return GetAxisComponent(A.Bounds.GetCenter(), BestAxis) < GetAxisComponent(B.Bounds.GetCenter(), BestAxis);
            });

        const int32 DesiredChildren = std::min<int32>(MeshBVHChildFanout, LeafCount);
        for (int32 RangeIndex = 0; RangeIndex < DesiredChildren; ++RangeIndex)
        {
            const int32 RangeStart = Start + (LeafCount * RangeIndex) / DesiredChildren;
            const int32 RangeEnd = Start + (LeafCount * (RangeIndex + 1)) / DesiredChildren;
            if (RangeStart >= RangeEnd)
            {
                continue;
            }

            const int32 ChildIdx = BuildRecursive(Mesh, RangeStart, RangeEnd);
            const int32 LocalChildIdx = Nodes[NodeIndex].ChildCount;

            Nodes[NodeIndex].Children[LocalChildIdx] = ChildIdx;

            const FBoundingBox& ChildBounds = Nodes[ChildIdx].Bounds;
            Nodes[NodeIndex].ChildMinX[LocalChildIdx] = ChildBounds.Min.X;
            Nodes[NodeIndex].ChildMinY[LocalChildIdx] = ChildBounds.Min.Y;
            Nodes[NodeIndex].ChildMinZ[LocalChildIdx] = ChildBounds.Min.Z;
            Nodes[NodeIndex].ChildMaxX[LocalChildIdx] = ChildBounds.Max.X;
            Nodes[NodeIndex].ChildMaxY[LocalChildIdx] = ChildBounds.Max.Y;
            Nodes[NodeIndex].ChildMaxZ[LocalChildIdx] = ChildBounds.Max.Z;

            Nodes[NodeIndex].ChildCount++;
        }
    }
    else
    {
        // 최적 축이 정해졌으면 해당 축의 bucket 순서대로 leaf를 재배치하고,
        // 연속된 bucket 구간마다 child node를 만듭니다.
        float MinCenter = FLT_MAX;
        float MaxCenter = -FLT_MAX;
        for (int32 LeafIndex = Start; LeafIndex < End; ++LeafIndex)
        {
            const float Center = GetAxisComponent(TriangleLeaves[LeafIndex].Bounds.GetCenter(), BestAxis);
            MinCenter = std::min(MinCenter, Center);
            MaxCenter = std::max(MaxCenter, Center);
        }

        const float Scale = static_cast<float>(MeshBVHChildFanout) / (MaxCenter - MinCenter);
        auto GetBucket = [BestAxis, MinCenter, Scale](const FTriangleLeaf& Leaf)
        {
            const float Center = GetAxisComponent(Leaf.Bounds.GetCenter(), BestAxis);
            int32 Bucket = static_cast<int32>((Center - MinCenter) * Scale);
            return std::clamp(Bucket, 0, MeshBVHChildFanout - 1);
        };

        std::sort(
            TriangleLeaves.begin() + Start,
            TriangleLeaves.begin() + End,
            [BestAxis, GetBucket](const FTriangleLeaf& A, const FTriangleLeaf& B)
            {
                const int32 BucketA = GetBucket(A);
                const int32 BucketB = GetBucket(B);
                if (BucketA != BucketB)
                {
                    return BucketA < BucketB;
                }
                const float CenterA = GetAxisComponent(A.Bounds.GetCenter(), BestAxis);
                const float CenterB = GetAxisComponent(B.Bounds.GetCenter(), BestAxis);
                if (CenterA != CenterB)
                {
                    return CenterA < CenterB;
                }
                return A.TriangleStartIndex < B.TriangleStartIndex;
            });

        // 모든 leaf가 같은 bucket에 몰리면 편향 재귀가 되므로 균등 분할 fallback으로 막습ㄴ디ㅏ.
        if (GetBucket(TriangleLeaves[Start]) == GetBucket(TriangleLeaves[End - 1]))
        {
            const int32 DesiredChildren = std::min<int32>(MeshBVHChildFanout, LeafCount);
            for (int32 RangeIndex = 0; RangeIndex < DesiredChildren; ++RangeIndex)
            {
                const int32 RangeStart = Start + (LeafCount * RangeIndex) / DesiredChildren;
                const int32 RangeEnd = Start + (LeafCount * (RangeIndex + 1)) / DesiredChildren;
                if (RangeStart >= RangeEnd)
                {
                    continue;
                }

                const int32 ChildIdx = BuildRecursive(Mesh, RangeStart, RangeEnd);
                const int32 LocalChildIdx = Nodes[NodeIndex].ChildCount;

                Nodes[NodeIndex].Children[LocalChildIdx] = ChildIdx;

                const FBoundingBox& ChildBounds = Nodes[ChildIdx].Bounds;
                Nodes[NodeIndex].ChildMinX[LocalChildIdx] = ChildBounds.Min.X;
                Nodes[NodeIndex].ChildMinY[LocalChildIdx] = ChildBounds.Min.Y;
                Nodes[NodeIndex].ChildMinZ[LocalChildIdx] = ChildBounds.Min.Z;
                Nodes[NodeIndex].ChildMaxX[LocalChildIdx] = ChildBounds.Max.X;
                Nodes[NodeIndex].ChildMaxY[LocalChildIdx] = ChildBounds.Max.Y;
                Nodes[NodeIndex].ChildMaxZ[LocalChildIdx] = ChildBounds.Max.Z;

                Nodes[NodeIndex].ChildCount++;
            }
        }
        else
        {
            int32 RangeStart = Start;
            while (RangeStart < End)
            {
                int32 RangeEnd = RangeStart + 1;
                const int32 Bucket = GetBucket(TriangleLeaves[RangeStart]);
                while (RangeEnd < End && GetBucket(TriangleLeaves[RangeEnd]) == Bucket)
                {
                    ++RangeEnd;
                }

                const int32 ChildIdx = BuildRecursive(Mesh, RangeStart, RangeEnd);
                const int32 LocalChildIdx = Nodes[NodeIndex].ChildCount;

                Nodes[NodeIndex].Children[LocalChildIdx] = ChildIdx;

                const FBoundingBox& ChildBounds = Nodes[ChildIdx].Bounds;
                Nodes[NodeIndex].ChildMinX[LocalChildIdx] = ChildBounds.Min.X;
                Nodes[NodeIndex].ChildMinY[LocalChildIdx] = ChildBounds.Min.Y;
                Nodes[NodeIndex].ChildMinZ[LocalChildIdx] = ChildBounds.Min.Z;
                Nodes[NodeIndex].ChildMaxX[LocalChildIdx] = ChildBounds.Max.X;
                Nodes[NodeIndex].ChildMaxY[LocalChildIdx] = ChildBounds.Max.Y;
                Nodes[NodeIndex].ChildMaxZ[LocalChildIdx] = ChildBounds.Max.Z;

                Nodes[NodeIndex].ChildCount++;
                RangeStart = RangeEnd;
            }
        }
    }

    // 사용하지 않는 child slot은 invalid AABB로 채워 SIMD 검사에서 자동으로 걸러지게 합니다.
    for (int32 i = Nodes[NodeIndex].ChildCount; i < 8; ++i)
    {
        Nodes[NodeIndex].ChildMinX[i] = 1e30f;
        Nodes[NodeIndex].ChildMinY[i] = 1e30f;
        Nodes[NodeIndex].ChildMinZ[i] = 1e30f;
        Nodes[NodeIndex].ChildMaxX[i] = -1e30f;
        Nodes[NodeIndex].ChildMaxY[i] = -1e30f;
        Nodes[NodeIndex].ChildMaxZ[i] = -1e30f;
    }

    return NodeIndex;
}
