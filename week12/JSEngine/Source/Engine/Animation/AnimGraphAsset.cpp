#include "AnimGraphAsset.h"
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
