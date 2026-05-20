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

	// ── Build API (low-level) ──
	FAnimGraphNode*  AddNode(EAnimGraphNodeType Type, const FName& DisplayName, float X, float Y);
	FAnimGraphPin*   AddPin(FAnimGraphNode& Node, EAnimGraphPinKind Kind, EAnimGraphPinType PinType, const FName& DisplayName);
	FAnimGraphLink*  AddLink(uint32 FromPinId, uint32 ToPinId);

	// ── Build API (high-level) ──
	// 노드 타입별 핀 레이아웃 default 까지 한 번에 박는 팩토리. UI 우클릭 메뉴 / InitializeDefault 가 사용.
	FAnimGraphNode*  AddNodeOfType(EAnimGraphNodeType Type, float X, float Y);

	// ── Edit API ──
	// 노드의 핀이 어느 link 에 걸려 있어도 모두 함께 cascade 삭제 (dangling pin id 방지).
	bool             RemoveNode(uint32 NodeId);
	bool             RemoveLink(uint32 LinkId);

	// 새로 생성된 비어있는 자산을 사용 가능한 상태로 초기화. 호출자는 CreateObject 직후 1회 호출.
	// 현재: OutputPose 1 + SequencePlayer 1 + 두 노드 Pose 연결선. 데이터 모델 1차 검증용.
	void             InitializeDefault();

	// ── Validation ──
	// pin id 한 쌍에 대해 링크 생성을 허용해도 되는지. UI 가 BeginCreate/QueryNewLink 응답에서 사용.
	// OutFrom / OutTo 는 input/output 방향이 자동 swap 된 결과 (드래그 방향 무관 검증).
	bool             CanLinkPins(uint32 PinAId, uint32 PinBId, uint32* OutFromPinId = nullptr, uint32* OutToPinId = nullptr) const;
	bool             HasOutputPoseNode() const;

	// ── Inspection ──
	const TArray<FAnimGraphNode>&  GetNodes() const { return Nodes; }
	const TArray<FAnimGraphLink>&  GetLinks() const { return Links; }

	FAnimGraphNode*       FindNode(uint32 NodeId);
	const FAnimGraphNode* FindNode(uint32 NodeId) const;
	FAnimGraphPin*        FindPin(uint32 PinId);
	const FAnimGraphPin*  FindPin(uint32 PinId) const;

	void Serialize(FArchive& Ar) override;

private:
	uint32 AllocateId() { return NextId++; }

	TArray<FAnimGraphNode> Nodes;
	TArray<FAnimGraphLink> Links;
	uint32                 NextId = 1; // 0 은 invalid sentinel
	FString                SourcePath;
};
