#include "Render/Submission/Atlas/AtlasBuddyAllocator2D.h"

#include <algorithm>

namespace
{
uint32 ClampShadowResolution(uint32 Resolution)
{
    if (Resolution <= 256u) return 256u;
    if (Resolution <= 512u) return 512u;
    if (Resolution <= 1024u) return 1024u;
    if (Resolution <= 2048u) return 2048u;
    return 4096u;
}

// 할당된 정사각형 블록 안쪽에 패딩을 적용한 실제 렌더 영역을 계산합니다.
FShadowAtlasRect MakeViewportRect(const FShadowAtlasRect& Rect)
{
    FShadowAtlasRect Viewport = Rect;
    const uint32 Padding = std::min(ShadowAtlas::Padding, Rect.Width / 2);
    Viewport.X += Padding;
    Viewport.Y += Padding;
    Viewport.Width = (Rect.Width > Padding * 2) ? (Rect.Width - Padding * 2) : Rect.Width;
    Viewport.Height = (Rect.Height > Padding * 2) ? (Rect.Height - Padding * 2) : Rect.Height;
    return Viewport;
}

// 셰이더가 atlas 내부 좌표를 바로 복원할 수 있도록 UV scale/offset을 계산합니다.
FVector4 MakeUVScaleOffset(const FShadowAtlasRect& ViewportRect)
{
    const float AtlasSize = static_cast<float>(ShadowAtlas::AtlasSize);
    return FVector4(
        static_cast<float>(ViewportRect.Width) / AtlasSize,
        static_cast<float>(ViewportRect.Height) / AtlasSize,
        static_cast<float>(ViewportRect.X) / AtlasSize,
        static_cast<float>(ViewportRect.Y) / AtlasSize);
}
} // namespace

FBuddyAllocator2D::FBuddyAllocator2D()
{
    Reset();
}

void FBuddyAllocator2D::Reset()
{
    for (ENodeState& Node : Nodes)
    {
        Node = ENodeState::Free;
    }
}

bool FBuddyAllocator2D::Allocate(uint32 Resolution, FShadowMapData& OutData)
{
    const uint32 TargetSize = ClampShadowResolution(Resolution);
    if (TargetSize < ShadowAtlas::MinResolution || TargetSize > ShadowAtlas::MaxResolution)
    {
        return false;
    }

    OutData = {};
    return AllocateRecursive(0, 0, 0, ShadowAtlas::AtlasSize, TargetSize, OutData);
}

void FBuddyAllocator2D::GatherAllocated(TArray<FShadowMapData>& OutAllocations) const
{
    GatherAllocatedRecursive(0, 0, 0, ShadowAtlas::AtlasSize, OutAllocations);
}

void FBuddyAllocator2D::Free(const FShadowMapData& Allocation)
{
    if (!Allocation.bAllocated || Allocation.NodeIndex == ShadowAtlas::InvalidNodeIndex || Allocation.NodeIndex >= NodeCount)
    {
        return;
    }

    FreeNode(Allocation.NodeIndex);
}

bool FBuddyAllocator2D::AllocateRecursive(
    uint32 NodeIndex,
    uint32 NodeX,
    uint32 NodeY,
    uint32 NodeSize,
    uint32 TargetSize,
    FShadowMapData& OutData)
{
    if (NodeIndex >= NodeCount || TargetSize > NodeSize)
    {
        return false;
    }

    ENodeState& Node = Nodes[NodeIndex];
    if (Node == ENodeState::Occupied)
    {
        return false;
    }

    if (TargetSize == NodeSize)
    {
        if (Node == ENodeState::Split)
        {
            return false;
        }

        Node = ENodeState::Occupied;
        OutData.bAllocated = true;
        OutData.NodeIndex = NodeIndex;
        OutData.Resolution = TargetSize;
        OutData.Rect = { NodeX, NodeY, NodeSize, NodeSize };
        OutData.ViewportRect = MakeViewportRect(OutData.Rect);
        OutData.UVScaleOffset = MakeUVScaleOffset(OutData.ViewportRect);
        return true;
    }

    if (IsLeafSize(NodeSize))
    {
        return false;
    }

    if (Node == ENodeState::Free)
    {
        Node = ENodeState::Split;
    }

    if (Node != ENodeState::Split)
    {
        return false;
    }

    const uint32 ChildSize = NodeSize / 2;
    for (uint32 ChildOffset = 0; ChildOffset < 4; ++ChildOffset)
    {
        const uint32 ChildIndex = GetChildIndex(NodeIndex, ChildOffset);
        const uint32 ChildX = NodeX + ((ChildOffset & 1u) ? ChildSize : 0u);
        const uint32 ChildY = NodeY + ((ChildOffset & 2u) ? ChildSize : 0u);
        if (AllocateRecursive(ChildIndex, ChildX, ChildY, ChildSize, TargetSize, OutData))
        {
            return true;
        }
    }

    TryMerge(NodeIndex);
    return false;
}

void FBuddyAllocator2D::GatherAllocatedRecursive(
    uint32 NodeIndex,
    uint32 NodeX,
    uint32 NodeY,
    uint32 NodeSize,
    TArray<FShadowMapData>& OutAllocations) const
{
    if (NodeIndex >= NodeCount)
    {
        return;
    }

    const ENodeState Node = Nodes[NodeIndex];
    if (Node == ENodeState::Occupied)
    {
        FShadowMapData Allocation = {};
        Allocation.bAllocated = true;
        Allocation.NodeIndex = NodeIndex;
        Allocation.Resolution = NodeSize;
        Allocation.Rect = { NodeX, NodeY, NodeSize, NodeSize };
        Allocation.ViewportRect = MakeViewportRect(Allocation.Rect);
        Allocation.UVScaleOffset = MakeUVScaleOffset(Allocation.ViewportRect);
        OutAllocations.push_back(Allocation);
        return;
    }

    if (Node != ENodeState::Split || IsLeafSize(NodeSize))
    {
        return;
    }

    const uint32 ChildSize = NodeSize / 2;
    for (uint32 ChildOffset = 0; ChildOffset < 4; ++ChildOffset)
    {
        const uint32 ChildIndex = GetChildIndex(NodeIndex, ChildOffset);
        const uint32 ChildX = NodeX + ((ChildOffset & 1u) ? ChildSize : 0u);
        const uint32 ChildY = NodeY + ((ChildOffset & 2u) ? ChildSize : 0u);
        GatherAllocatedRecursive(ChildIndex, ChildX, ChildY, ChildSize, OutAllocations);
    }
}

void FBuddyAllocator2D::FreeNode(uint32 NodeIndex)
{
    Nodes[NodeIndex] = ENodeState::Free;
    if (NodeIndex != 0)
    {
        TryMerge(GetParentIndex(NodeIndex));
    }
}

void FBuddyAllocator2D::TryMerge(uint32 NodeIndex)
{
    if (NodeIndex >= NodeCount || Nodes[NodeIndex] != ENodeState::Split)
    {
        return;
    }

    bool bAllChildrenFree = true;
    for (uint32 ChildOffset = 0; ChildOffset < 4; ++ChildOffset)
    {
        const uint32 ChildIndex = GetChildIndex(NodeIndex, ChildOffset);
        if (ChildIndex >= NodeCount || Nodes[ChildIndex] != ENodeState::Free)
        {
            bAllChildrenFree = false;
            break;
        }
    }

    if (!bAllChildrenFree)
    {
        return;
    }

    Nodes[NodeIndex] = ENodeState::Free;
    if (NodeIndex != 0)
    {
        TryMerge(GetParentIndex(NodeIndex));
    }
}

uint32 FBuddyAllocator2D::GetChildIndex(uint32 ParentIndex, uint32 ChildOffset)
{
    return ParentIndex * 4 + 1 + ChildOffset;
}

uint32 FBuddyAllocator2D::GetParentIndex(uint32 NodeIndex)
{
    return (NodeIndex - 1) / 4;
}

bool FBuddyAllocator2D::IsLeafSize(uint32 NodeSize)
{
    return NodeSize <= ShadowAtlas::MinResolution;
}
