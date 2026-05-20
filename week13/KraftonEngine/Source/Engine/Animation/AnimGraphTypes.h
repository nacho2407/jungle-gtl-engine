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

// ── StateMachine 노드 보조 자료구조 ──
// 평면 그래프에 별도 노드로 표현하지 않고 StateMachine 노드 안에 nested 보유.
// 후속 단계에서 sub-graph view (UE 더블클릭 진입) 도입 시 동일 자료가 재사용됨.

enum class ETransitionOp : uint8
{
	Greater,       // var >  threshold
	GreaterEqual,  // var >= threshold
	Less,          // var <  threshold
	LessEqual,     // var <= threshold
	Equal,         // |var - threshold| < eps
	NotEqual
};

struct FAnimGraphState
{
	FName    StateName;
	FString  SequencePath; // 이 state 가 재생할 sequence (디스크 path). LoadAnimation 으로 해상.
	float    PlayRate    = 1.0f;
	bool     bLooping    = true;

	// Sub-state-machine — 그래프 안의 다른 StateMachine 노드를 가리킴. 0 == 없음 (일반 sequence state).
	// 컴파일러가 그 노드를 컴파일해 UAnimState::SubGraphOverride 에 박음 → state Enter 시 sub-tree
	// OnBecomeRelevant. UE 의 nested state machine 동등.
	// SubGraphNodeId 가 있으면 SequencePath 는 무시.
	uint32   SubGraphNodeId = 0;

	friend FArchive& operator<<(FArchive& Ar, FAnimGraphState& State);
};

struct FAnimGraphTransition
{
	FName         FromStateName; // FName::None == AnyState
	FName         ToStateName;
	FName         VariableName;  // OwnerClass 의 UPROPERTY 이름 (Float/Int/Bool 등)
	ETransitionOp Op            = ETransitionOp::Greater;
	float         Threshold     = 0.0f;
	float         BlendTime     = 0.2f;

	friend FArchive& operator<<(FArchive& Ar, FAnimGraphTransition& T);
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

	// SequencePlayer 옵션. PlayRate / bLooping — 노드 inspector 에서 편집.
	float                  PlayRate    = 1.0f;
	bool                   bLooping    = true;

	// Slot 노드의 montage slot name (비어있으면 컴파일러가 UAnimInstance::DefaultMontageSlot 으로 fallback).
	FName                  SlotName;

	// LayeredBlendPerBone 의 BlendPose 전체 contribution. F-1 한정 — PerBoneMask 는 컴파일 시점에
	// mesh 의 모든 본 true 로 채움 (full blend). 후속 단계에서 root bone name 기반 부분 mask 추가.
	float                  BlendWeight = 1.0f;

	// VariableGet 노드 — UAnimInstance 자식 클래스의 어떤 UPROPERTY 를 매 frame 읽을지.
	// inspector 에서 asset 의 OwnerClassName 기반 dropdown 으로 선택.
	// 컴파일러는 이 노드를 별도 런타임 노드로 만들지 않고, consumer 노드 (BlendListByEnum 등) 의
	// 람다로 inline — 그래프 시각화 ↔ 런타임 트리 디커플.
	FName                  VariableName;

	// StateMachine 노드 — states / transitions / initial state 를 nested 보유.
	// 평면 그래프에선 state 별 노드 표현 없음 (inspector form 에서 정의). 후속에서 sub-graph
	// 더블클릭 진입 시 동일 자료가 재사용됨.
	TArray<FAnimGraphState>      States;
	TArray<FAnimGraphTransition> Transitions;
	FName                        InitialStateName;

	friend FArchive& operator<<(FArchive& Ar, FAnimGraphNode& Node);
};
