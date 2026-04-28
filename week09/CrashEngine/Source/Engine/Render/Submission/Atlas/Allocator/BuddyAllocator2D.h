#pragma once

#include "Render/Submission/Atlas/ShadowAtlasTypes.h"

// 2D 정사각형 영역을 quadtree 형태로 관리하는 buddy allocator입니다.
// 각 노드는 atlas 안의 정사각형 한 칸을 의미합니다.
// - Free: 비어 있는 노드
// - Split: 자식 4개로 쪼개져 자식이 공간을 관리하는 내부 노드
// - Occupied: 현재 노드 크기 그대로 할당이 끝난 노드
// 할당 시에는 정확히 맞는 크기의 노드를 찾을 때까지 4등분하며 내려가고,
// 해제 시에는 형제 4개가 모두 Free이면 부모를 다시 Free로 합칩니다.
class FBuddyAllocator2D
{
public:
    FBuddyAllocator2D();

    void Reset();
    bool Allocate(uint32 Resolution, FShadowMapData& OutData);
    void Free(const FShadowMapData& Allocation);
    void GatherAllocated(TArray<FShadowMapData>& OutAllocations) const;

private:
    enum class ENodeState : uint8
    {
        Free,
        Split,
        Occupied
    };

    static constexpr uint32 MaxDepth = 4;
    static constexpr uint32 NodeCount = 341;

    bool AllocateRecursive(uint32 NodeIndex, uint32 NodeX, uint32 NodeY, uint32 NodeSize, uint32 TargetSize, FShadowMapData& OutData);
    void GatherAllocatedRecursive(uint32 NodeIndex, uint32 NodeX, uint32 NodeY, uint32 NodeSize, TArray<FShadowMapData>& OutAllocations) const;
    void FreeNode(uint32 NodeIndex);
    void TryMerge(uint32 NodeIndex);
    static uint32 GetChildIndex(uint32 ParentIndex, uint32 ChildOffset);
    static uint32 GetParentIndex(uint32 NodeIndex);
    static bool   IsLeafSize(uint32 NodeSize);

private:
    ENodeState Nodes[NodeCount] = {};
};
