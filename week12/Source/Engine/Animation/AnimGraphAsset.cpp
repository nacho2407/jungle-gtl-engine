#include "AnimGraphAsset.h"
#include "Core/Logging/Log.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

void UAnimGraphAsset::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
}

const FAnimGraphNodeDesc* UAnimGraphAsset::FindNode(int32 NodeId) const
{
    for (const FAnimGraphNodeDesc& Node : Nodes)
    {
        if (Node.NodeId == NodeId) return &Node;
    }
    return nullptr;
}

FAnimGraphNodeDesc* UAnimGraphAsset::FindNode(int32 NodeId)
{
    for (FAnimGraphNodeDesc& Node : Nodes)
    {
        if (Node.NodeId == NodeId) return &Node;
    }
    return nullptr;
}

bool UAnimGraphAsset::ValidateAndRepairGraph()
{
    bool bChanged = false;

    std::unordered_set<int32> UsedIds;
    std::unordered_map<int32, int32> RemappedIds;
    int32 NextId = 1;

    for (FAnimGraphNodeDesc& Node : Nodes)
    {
        const int32 OldId = Node.NodeId;
        const bool bValidUniqueId = OldId > 0 && UsedIds.find(OldId) == UsedIds.end();
        if (bValidUniqueId)
        {
            UsedIds.insert(OldId);
            NextId = std::max(NextId, OldId + 1);
            continue;
        }

        while (UsedIds.find(NextId) != UsedIds.end())
        {
            ++NextId;
        }

        Node.NodeId = NextId;
        UsedIds.insert(Node.NodeId);
        if (OldId > 0)
        {
            RemappedIds[OldId] = Node.NodeId;
        }
        ++NextId;
        bChanged = true;
    }

    auto RemapNodeId = [&RemappedIds](int32& NodeId)
    {
        auto It = RemappedIds.find(NodeId);
        if (It != RemappedIds.end())
        {
            NodeId = It->second;
            return true;
        }
        return false;
    };

    if (RemapNodeId(RootNodeId))
    {
        bChanged = true;
    }

    for (FAnimGraphNodeDesc& Node : Nodes)
    {
        if (RemapNodeId(Node.InputPoseNodeId))
        {
            bChanged = true;
        }
    }

    const FAnimGraphNodeDesc* RootNode = FindNode(RootNodeId);
    if (!RootNode || RootNode->Type != EAnimGraphNodeType::OutputPose)
    {
        RootNodeId = -1;
        for (const FAnimGraphNodeDesc& Node : Nodes)
        {
            if (Node.Type == EAnimGraphNodeType::OutputPose)
            {
                RootNodeId = Node.NodeId;
                break;
            }
        }
        bChanged = true;
    }

    for (FAnimGraphNodeDesc& Node : Nodes)
    {
        if (Node.Type != EAnimGraphNodeType::OutputPose)
        {
            continue;
        }

        if (Node.InputPoseNodeId >= 0 && FindNode(Node.InputPoseNodeId))
        {
            continue;
        }

        Node.InputPoseNodeId = -1;
        for (const FAnimGraphNodeDesc& Candidate : Nodes)
        {
            if (Candidate.NodeId != Node.NodeId && Candidate.Type != EAnimGraphNodeType::OutputPose)
            {
                Node.InputPoseNodeId = Candidate.NodeId;
                bChanged = true;
                break;
            }
        }
    }

    if (bChanged)
    {
        UE_LOG_WARNING(
            "[AnimGraphAsset] Graph repaired after load | RootNodeId=%d | Nodes=%d",
            RootNodeId,
            static_cast<int32>(Nodes.size()));
    }

    return bChanged;
}
