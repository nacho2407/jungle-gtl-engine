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

	// 이 그래프가 어떤 AnimInstance 자식 클래스용인지 — VariableGet 노드의 변수 dropdown 이
	// 이 클래스의 UPROPERTY 목록을 보여줌. 기본값 "UAnimInstance" — Speed 같은 자식 변수는
	// 못 보이지만 RootMotionMode 같은 base 변수는 가능.
	void           SetOwnerClassName(const FString& InName) { OwnerClassName = InName; }
	const FString& GetOwnerClassName() const                { return OwnerClassName; }

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

	// 자산을 코드에서 외부 데이터 (e.g. mock sequence) 로 채울 때 노드 타입별 첫 인스턴스 접근용.
	FAnimGraphNode*       FindFirstNodeOfType(EAnimGraphNodeType Type);

	void Serialize(FArchive& Ar) override;

	// ── Live preview / 재컴파일 트리거 ──
	// UAnimGraphInstance 가 매 frame 비교 → 다르면 자기 RootNode 재컴파일.
	// Add/Remove/Set 류 build·edit API 가 자동 호출. 노드 inspector 가 노드 멤버를 직접 수정한
	// 후엔 외부에서 BumpVersion() 명시 호출 필요 (Asset 은 그 변경을 모름).
	// transient — 디스크에서 load 한 자산은 0 으로 시작. 호환성 영향 0.
	uint32 GetVersion() const { return Version; }
	void   BumpVersion()      { ++Version; }

private:
	uint32 AllocateId() { return NextId++; }

	TArray<FAnimGraphNode> Nodes;
	TArray<FAnimGraphLink> Links;
	uint32                 NextId = 1; // 0 은 invalid sentinel
	uint32                 Version = 0;
	FString                SourcePath;
	FString                OwnerClassName = "UAnimInstance";
};
