#include "Collision/BVH/ScenePrimitiveBVH.h"

#include "Collision/RayUtils.h"
#include "Collision/RayUtilsSIMD.h"
#include "Math/Intersection.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "GameFramework/AActor.h"

#include <algorithm>
#include <bit>

namespace
{
constexpr int32 ScenePrimitiveBVHMaxTraversalStack = 512;
} // namespace

void FScenePrimitiveBVH::MarkDirty()
{
    bDirty = true;
}

void FScenePrimitiveBVH::BuildNow(const TArray<AActor*>& Actors)
{
    Leaves.clear();
    BVH.Reset();
    PrimitivePackets.clear();

    for (AActor* Actor : Actors)
    {
        if (!Actor || !Actor->IsVisible())
        {
            continue;
        }

        for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
        {
            if (!Primitive || !Primitive->ShouldRenderInCurrentWorld())
            {
                continue;
            }

            if (Primitive->IsEditorHelper())
            {
                continue;
            }

            FLeaf Leaf;
            Leaf.Primitive = Primitive;
            Leaf.StaticMeshPrimitive = Cast<UStaticMeshComponent>(Primitive);
            Leaf.Owner = Actor;
            Leaf.Bounds = Primitive->GetWorldBoundingBox();

            if (!Leaf.Bounds.IsValid())
            {
                continue;
            }

            Leaves.push_back(Leaf);
        }
    }

    if (!Leaves.empty())
    {
        PrimitivePackets.reserve((static_cast<int32>(Leaves.size()) + LeafPacketSize - 1) / LeafPacketSize);
        BVH.Build(
            Leaves,
            [](const FLeaf& Leaf) { return Leaf.Bounds; },
            [](const FLeaf& Leaf)
            {
                return reinterpret_cast<uintptr_t>(Leaf.Primitive);
            });

        const TArray<FNode>& Nodes = BVH.GetNodes();
        for (const FNode& Node : Nodes)
        {
            if (!Node.IsLeaf())
            {
                continue;
            }

            FNode& MutableNode = const_cast<FNode&>(Node);
            MutableNode.PayloadIndex = static_cast<int32>(PrimitivePackets.size());
            MutableNode.PayloadCount = (Node.LeafCount + LeafPacketSize - 1) / LeafPacketSize;

            for (int32 PacketIndex = 0; PacketIndex < MutableNode.PayloadCount; ++PacketIndex)
            {
                const int32 PacketStart = Node.FirstLeaf + PacketIndex * LeafPacketSize;
                const int32 PacketEnd = (std::min)(PacketStart + LeafPacketSize, Node.FirstLeaf + Node.LeafCount);

                FPrimitivePacket Packet{};
                Packet.PrimitiveCount = PacketEnd - PacketStart;

                for (int32 LocalIndex = 0; LocalIndex < LeafPacketSize; ++LocalIndex)
                {
                    if (LocalIndex < Packet.PrimitiveCount)
                    {
                        const int32 LeafIndex = PacketStart + LocalIndex;
                        const FBoundingBox& LeafBounds = Leaves[LeafIndex].Bounds;
                        Packet.PrimitiveIndices[LocalIndex] = LeafIndex;
                        Packet.MinX[LocalIndex] = LeafBounds.Min.X;
                        Packet.MinY[LocalIndex] = LeafBounds.Min.Y;
                        Packet.MinZ[LocalIndex] = LeafBounds.Min.Z;
                        Packet.MaxX[LocalIndex] = LeafBounds.Max.X;
                        Packet.MaxY[LocalIndex] = LeafBounds.Max.Y;
                        Packet.MaxZ[LocalIndex] = LeafBounds.Max.Z;
                    }
                    else
                    {
                        Packet.MinX[LocalIndex] = 1e30f;
                        Packet.MinY[LocalIndex] = 1e30f;
                        Packet.MinZ[LocalIndex] = 1e30f;
                        Packet.MaxX[LocalIndex] = -1e30f;
                        Packet.MaxY[LocalIndex] = -1e30f;
                        Packet.MaxZ[LocalIndex] = -1e30f;
                    }
                }

                PrimitivePackets.push_back(Packet);
            }
        }
    }

    bDirty = false;
}

void FScenePrimitiveBVH::EnsureBuilt(const TArray<AActor*>& Actors)
{
    if (!bDirty)
    {
        return;
    }

    BuildNow(Actors);
}

bool FScenePrimitiveBVH::Raycast(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const
{
    struct FTraversalEntry
    {
        int32 NodeIndex = -1;
        float TMin = 0.0f;
    };

    OutHitResult = {};
    OutActor = nullptr;

    const TArray<FNode>& Nodes = BVH.GetNodes();
    if (Nodes.empty())
    {
        return false;
    }

    float RootTMin = 0.0f;
    float RootTMax = 0.0f;
    if (!FMath::IntersectRayAABB(Ray, Nodes[0].Bounds, RootTMin, RootTMax))
    {
        return false;
    }

    const FRaySIMDContext RayContext = FRayUtilsSIMD::MakeRayContext(Ray.Origin, Ray.Direction);

    FTraversalEntry NodeStack[ScenePrimitiveBVHMaxTraversalStack];
    int32 StackSize = 0;
    NodeStack[StackSize++] = { 0, RootTMin };

    while (StackSize > 0)
    {
        const FTraversalEntry Entry = NodeStack[--StackSize];
        if (Entry.TMin > OutHitResult.Distance)
        {
            continue;
        }

        const FNode& Node = Nodes[Entry.NodeIndex];
        if (Node.IsLeaf())
        {
            FTraversalEntry PrimitiveEntries[MaxLeafSize];
            int32 PrimitiveEntryCount = 0;

            for (int32 PacketIndex = 0; PacketIndex < Node.PayloadCount; ++PacketIndex)
            {
                const FPrimitivePacket& Packet = PrimitivePackets[Node.PayloadIndex + PacketIndex];
                alignas(32) float PrimitiveTMinValues[8];
                const int32 PrimitiveMask = FRayUtilsSIMD::IntersectAABB8(
                    RayContext,
                    Packet.MinX, Packet.MinY, Packet.MinZ,
                    Packet.MaxX, Packet.MaxY, Packet.MaxZ,
                    OutHitResult.Distance,
                    PrimitiveTMinValues);

                if (PrimitiveMask == 0)
                {
                    continue;
                }

                uint32 RemainingPrimitiveMask = static_cast<uint32>(PrimitiveMask) & ((1u << Packet.PrimitiveCount) - 1u);
                while (RemainingPrimitiveMask != 0)
                {
                    const uint32 Lane = std::countr_zero(RemainingPrimitiveMask);
                    PrimitiveEntries[PrimitiveEntryCount++] = { Packet.PrimitiveIndices[Lane], PrimitiveTMinValues[Lane] };
                    RemainingPrimitiveMask &= (RemainingPrimitiveMask - 1);
                }
            }

            if (PrimitiveEntryCount > 1)
            {
                if (PrimitiveEntryCount == 2)
                {
                    if (PrimitiveEntries[1].TMin < PrimitiveEntries[0].TMin)
                    {
                        std::swap(PrimitiveEntries[0], PrimitiveEntries[1]);
                    }
                }
                else
                {
                    for (int32 I = 1; I < PrimitiveEntryCount; ++I)
                    {
                        FTraversalEntry Key = PrimitiveEntries[I];
                        int32 J = I - 1;
                        while (J >= 0 && PrimitiveEntries[J].TMin > Key.TMin)
                        {
                            PrimitiveEntries[J + 1] = PrimitiveEntries[J];
                            --J;
                        }
                        PrimitiveEntries[J + 1] = Key;
                    }
                }
            }

            for (int32 EntryIndex = 0; EntryIndex < PrimitiveEntryCount; ++EntryIndex)
            {
                if (PrimitiveEntries[EntryIndex].TMin >= OutHitResult.Distance)
                {
                    continue;
                }

                const FLeaf& Leaf = Leaves[PrimitiveEntries[EntryIndex].NodeIndex];
                FHitResult CandidateHit{};
                bool bHit = false;

                if (UStaticMeshComponent* const StaticMeshComponent = Leaf.StaticMeshPrimitive)
                {
                    const FMatrix& WorldMatrix = StaticMeshComponent->GetWorldMatrix();
                    const FMatrix& WorldInverse = StaticMeshComponent->GetWorldInverseMatrix();
                    bHit = StaticMeshComponent->LineTraceStaticMeshFast(Ray, WorldMatrix, WorldInverse, CandidateHit);
                }
                else
                {
                    bHit = Leaf.Primitive->LineTraceComponent(Ray, CandidateHit);
                }

                if (bHit && CandidateHit.Distance < OutHitResult.Distance)
                {
                    OutHitResult = CandidateHit;
                    OutActor = Leaf.Owner;
                }
            }
            continue;
        }

        alignas(32) float TMinValues[8];
        const int32 Mask = FRayUtilsSIMD::IntersectAABB8(
            RayContext,
            Node.ChildMinX, Node.ChildMinY, Node.ChildMinZ,
            Node.ChildMaxX, Node.ChildMaxY, Node.ChildMaxZ,
            OutHitResult.Distance,
            TMinValues);
        if (Mask == 0)
        {
            continue;
        }

        FTraversalEntry ChildEntries[ChildFanout];
        int32 ChildEntryCount = 0;

        uint32 RemainingChildMask = static_cast<uint32>(Mask) & ((1u << Node.ChildCount) - 1u);
        while (RemainingChildMask != 0)
        {
            const uint32 Lane = std::countr_zero(RemainingChildMask);
            ChildEntries[ChildEntryCount++] = { Node.Children[Lane], TMinValues[Lane] };
            RemainingChildMask &= (RemainingChildMask - 1);
        }

        if (ChildEntryCount == 1)
        {
            if (StackSize < ScenePrimitiveBVHMaxTraversalStack)
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

        for (int32 I = 0; I < ChildEntryCount && StackSize < ScenePrimitiveBVHMaxTraversalStack; ++I)
        {
            NodeStack[StackSize++] = ChildEntries[I];
        }
    }

    return OutActor != nullptr;
}
