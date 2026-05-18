#pragma once
#include "Core/CoreMinimal.h"
#include "Object/Object.h"
#include "Animation/AnimGraphNode.h"

UCLASS()
class UAnimGraphAsset: public UObject
{
public:
	GENERATED_BODY(UAnimGraphAsset, UObject)

	virtual void Serialize(FArchive& Ar) override;

	TArray<FAnimGraphNodeDesc> Nodes;
	int32 RootNodeId = -1;

	const FAnimGraphNodeDesc* FindNode(int32 NodeId) const;
	FAnimGraphNodeDesc* FindNode(int32 NodeId);
};
