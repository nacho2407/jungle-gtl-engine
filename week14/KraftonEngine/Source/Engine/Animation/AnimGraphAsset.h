#pragma once

#include "Object/Object.h"
#include "Core/CoreTypes.h"
#include "Animation/AnimGraphTypes.h"

#include "Source/Engine/Animation/AnimGraphAsset.generated.h"

class FArchive;

// AnimGraph (시각 노드 그래프) 자산.
// 데이터 모델만 보유 — 런타임 FAnimNode_* 트리 컴파일은 후속 단계에서 별도 컴파일러가 담당.
//
// id 정책: Nodes / Pins / Links 가 동일 NextId 공간에서 발급 (imgui-node-editor 가 link 의
// 양 끝 pin id 를 같은 namespace 로 식별). id 0 은 invalid sentinel — 절대 발급 X.
UCLASS()
class UAnimGraphAsset : public UObject
{
public:
	GENERATED_BODY()
	UAnimGraphAsset() = default;
	~UAnimGraphAsset() override = default;

	void           SetSourcePath(const FString& InPath) { SourcePath = InPath; }
	const FString& GetSourcePath() const                { return SourcePath; }

	// ── Build API ──
	FAnimGraphNode*  AddNode(EAnimGraphNodeType Type, const FName& DisplayName, float X, float Y);
	FAnimGraphPin*   AddPin(FAnimGraphNode& Node, EAnimGraphPinKind Kind, EAnimGraphPinType PinType, const FName& DisplayName);
	FAnimGraphLink*  AddLink(uint32 FromPinId, uint32 ToPinId);

	// 새로 생성된 비어있는 자산을 사용 가능한 상태로 초기화. 호출자는 CreateObject 직후 1회 호출.
	// 현재: OutputPose 1 + SequencePlayer 1 + 두 노드 Pose 연결선. 데이터 모델 1차 검증용.
	void             InitializeDefault();

	// ── Inspection ──
	const TArray<FAnimGraphNode>&  GetNodes() const { return Nodes; }
	const TArray<FAnimGraphLink>&  GetLinks() const { return Links; }

	FAnimGraphNode*  FindNode(uint32 NodeId);
	FAnimGraphPin*   FindPin(uint32 PinId);
	const FAnimGraphPin* FindPin(uint32 PinId) const;

	void Serialize(FArchive& Ar) override;

private:
	uint32 AllocateId() { return NextId++; }

	TArray<FAnimGraphNode> Nodes;
	TArray<FAnimGraphLink> Links;
	uint32                 NextId = 1; // 0 은 invalid sentinel
	FString                SourcePath;
};
