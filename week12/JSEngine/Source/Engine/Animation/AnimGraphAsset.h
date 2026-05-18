#pragma once

#include "Animation/AnimGraphNode.h"
#include "Core/CoreMinimal.h"
#include "Object/Object.h"

UCLASS()
class UAnimGraphAsset : public UObject
{
public:
	GENERATED_BODY(UAnimGraphAsset, UObject)

	void Serialize(FArchive& Ar) override;

	UPROPERTY()
	TArray<FAnimGraphNodeDesc> Nodes;
	UPROPERTY()
	int32 RootNodeId = -1;

	const FAnimGraphNodeDesc* FindNode(int32 NodeId) const;
	FAnimGraphNodeDesc* FindNode(int32 NodeId);
};
