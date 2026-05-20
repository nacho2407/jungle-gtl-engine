#pragma once

#include "Core/CoreTypes.h"
#include "Object/FName.h"

class FArchive;
class UAnimSequenceBase;

// AnimGraph 자산의 정적 데이터 모델 — 런타임 FAnimNode_* 트리와는 분리.
// 컴파일 단계에서 이 그래프를 위상정렬 → MakeNode<T> → SetRootNode 트리를 build.

enum class EAnimGraphPinKind : uint8
{
	Input,
	Output
};

// 단계 1 은 Pose 만 실질 사용. Float/Bool/Int/Name 은 후속 VariableGet 노드 대비 미리 정의.
enum class EAnimGraphPinType : uint8
{
	Pose,
	Float,
	Bool,
	Int,
	Name
};

// FAnimNode_* 와 1:1 매핑되는 enum. 단계 1 은 OutputPose / SequencePlayer 만 실질 사용,
// 나머지는 후속 단계에서 노드 팩토리가 핀 레이아웃 생성 시 분기 키로 사용.
enum class EAnimGraphNodeType : uint8
{
	OutputPose,           // FAnimNode_Root 와 매핑 — 그래프 종착점
	SequencePlayer,
	StateMachine,
	Slot,
	LayeredBlendPerBone,
	BlendListByEnum,
	VariableGet,          // UAnimInstance UPROPERTY 참조 — 미구현
};

struct FAnimGraphPin
{
	// 같은 자산 안에서 Node/Pin/Link 가 같은 id 공간을 공유 (imgui-node-editor 가
	// link 양 끝의 pin id 를 동일 namespace 로 식별하기 위함). 0 == invalid.
	uint32             PinId        = 0;
	uint32             OwningNodeId = 0;
	EAnimGraphPinKind  Kind         = EAnimGraphPinKind::Input;
	EAnimGraphPinType  Type         = EAnimGraphPinType::Pose;
	FName              DisplayName;

	friend FArchive& operator<<(FArchive& Ar, FAnimGraphPin& Pin);
};

struct FAnimGraphLink
{
	uint32 LinkId    = 0;
	uint32 FromPinId = 0; // Output 쪽 핀
	uint32 ToPinId   = 0; // Input 쪽 핀

	friend FArchive& operator<<(FArchive& Ar, FAnimGraphLink& Link);
};

struct FAnimGraphNode
{
	uint32                 NodeId = 0;
	EAnimGraphNodeType     Type   = EAnimGraphNodeType::OutputPose;
	FName                  DisplayName;
	float                  PosX   = 0.0f;
	float                  PosY   = 0.0f;
	TArray<FAnimGraphPin>  Pins;

	// SequencePlayer 노드의 입력 시퀀스 — 컴파일러가 FAnimNode_SequencePlayer::Sequence 로 박음.
	// 다른 노드 타입에선 미사용. raw pointer + transient — 자산은 SequencePath 만 보유.
	UAnimSequenceBase*     SequenceRef = nullptr;

	// 직렬화 가능한 sequence 식별자. UAnimGraphInstance::NativeInitializeAnimation 가
	// FAnimationManager::LoadAnimation 으로 해상해 SequenceRef 에 박는다.
	// empty / "None" 이면 UAnimGraphInstance::DefaultSequencePath 가 fallback.
	FString                SequencePath;

	// SequencePlayer 옵션. PlayRate / bLooping — 노드 inspector 도입 시 편집 (단계 E).
	float                  PlayRate    = 1.0f;
	bool                   bLooping    = true;

	friend FArchive& operator<<(FArchive& Ar, FAnimGraphNode& Node);
};
