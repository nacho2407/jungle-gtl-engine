#include "AnimGraphCompiler.h"

#include "Animation/Graph/AnimGraphAsset.h"
#include "Animation/Graph/AnimGraphTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/Nodes/AnimNode_Base.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Animation/StateMachine/AnimState.h"
#include "Animation/AnimationManager.h"
#include "Animation/Nodes/AnimNode_BlendListByEnum.h"
#include "Animation/Nodes/AnimNode_LayeredBlendPerBone.h"
#include "Animation/Nodes/AnimNode_RefPose.h"
#include "Animation/Nodes/AnimNode_SequencePlayer.h"
#include "Animation/Nodes/AnimNode_Slot.h"
#include "Animation/Nodes/AnimNode_StateMachine.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Logging/Log.h"
#include "Core/Types/PropertyTypes.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Object/Reflection/UClass.h"

#include <cmath>

namespace
{
	// 컴파일 재귀 깊이 가드 — sub-state-machine 가 서로 가리키는 cycle 또는 의도치 않은
	// 깊은 nesting 으로 stack overflow 가는 것을 방지. V1 nesting 의도는 1~2 단계.
	constexpr int32 kMaxCompileDepth = 16;
	thread_local int32 g_CompileDepth = 0;

	struct FDepthScope
	{
		FDepthScope()  { ++g_CompileDepth; }
		~FDepthScope() { --g_CompileDepth; }
	};

	// graph 의 한 노드 → FAnimNode_* 인스턴스. 재귀 — 입력 핀의 source 를 따라 자식 build.
	// nullptr 반환은 컴파일 실패 (호출 chain 어디선가 미지원 / dangling).
	FAnimNode_Base* CompileNode(const UAnimGraphAsset& Graph, UAnimInstance& Owner, const FAnimGraphNode& Node);

	// 노드 안에서 핀 이름으로 input 핀 찾기 (Slot/LayeredBlend 처럼 input 이 여러 개인 노드용).
	const FAnimGraphPin* FindInputPinByName(const FAnimGraphNode& Node, const FName& Name)
	{
		for (const FAnimGraphPin& Pin : Node.Pins)
		{
			if (Pin.Kind == EAnimGraphPinKind::Input && Pin.DisplayName == Name) return &Pin;
		}
		return nullptr;
	}

	// input pin 에 연결된 source pin 의 owning node 를 그대로 반환 (컴파일 X). 데이터 흐름
	// (VariableGet → BlendListByEnum.Selector) 처럼 source 메타데이터만 필요한 경우.
	const FAnimGraphNode* FindInputSourceNode(const UAnimGraphAsset& Graph, uint32 InputPinId)
	{
		if (InputPinId == 0) return nullptr;
		for (const FAnimGraphLink& L : Graph.GetLinks())
		{
			if (L.ToPinId != InputPinId) continue;
			const FAnimGraphPin* SrcPin = Graph.FindPin(L.FromPinId);
			if (!SrcPin) return nullptr;
			return Graph.FindNode(SrcPin->OwningNodeId);
		}
		return nullptr;
	}

	// 매 frame UAnimInstance 의 UPROPERTY 한 개를 float 으로 읽는 람다 빌드.
	// Float / Int / Bool / ByteBool 만 지원 — 그 외 타입은 0 반환.
	// FProperty* 캐싱은 람다 capture 안에서 lazy lookup (Initialize 시점 hook 없음).
	TFunction<float(UAnimInstance*)> MakeFloatReader(FName VariableName)
	{
		return [VariableName](UAnimInstance* AI) -> float
		{
			if (!AI || VariableName == FName::None) return 0.0f;

			UClass* Cls = AI->GetClass();
			if (!Cls) return 0.0f;

			TArray<const FProperty*> Props;
			Cls->GetPropertyRefs(Props);

			const FString VarStr = VariableName.ToString();
			for (const FProperty* Prop : Props)
			{
				if (!Prop || !Prop->Name) continue;
				if (VarStr != Prop->Name) continue;

				void* Ptr = Prop->GetValuePtrFor(AI);
				if (!Ptr) return 0.0f;

				switch (Prop->GetType())
				{
					case EPropertyType::Float:    return *static_cast<float*>(Ptr);
					case EPropertyType::Int:      return static_cast<float>(*static_cast<int32*>(Ptr));
					case EPropertyType::Bool:     return *static_cast<bool*>(Ptr) ? 1.0f : 0.0f;
					case EPropertyType::ByteBool: return *static_cast<uint8*>(Ptr) ? 1.0f : 0.0f;
					default:                      return 0.0f;
				}
			}
			return 0.0f;
		};
	}

	// Transition rule — 단일 비교식 (Var Op Threshold) 을 bool 람다로 빌드.
	// F-3 단순화 — 미니 그래프 / 다중 조건 / && || 는 후속 단계.
	TFunction<bool(UAnimInstance*)> MakeTransitionCondition(FName VarName, ETransitionOp Op, float Threshold)
	{
		auto Reader = MakeFloatReader(VarName);
		return [Reader, Op, Threshold](UAnimInstance* AI) -> bool
		{
			const float V = Reader(AI);
			constexpr float Eps = 1e-4f;
			switch (Op)
			{
				case ETransitionOp::Greater:      return V >  Threshold;
				case ETransitionOp::GreaterEqual: return V >= Threshold;
				case ETransitionOp::Less:         return V <  Threshold;
				case ETransitionOp::LessEqual:    return V <= Threshold;
				case ETransitionOp::Equal:        return std::fabs(V - Threshold) < Eps;
				case ETransitionOp::NotEqual:     return std::fabs(V - Threshold) >= Eps;
			}
			return false;
		};
	}

	// in-pin 에 연결된 단일 source pin → 그 owning node 를 컴파일해 반환.
	// 단계 D1 에선 input 핀당 하나의 source 만 가정 (multi-fanout 의 fan-in 은 미지원).
	FAnimNode_Base* CompileInputPose(const UAnimGraphAsset& Graph, UAnimInstance& Owner, uint32 InputPinId)
	{
		if (InputPinId == 0) return nullptr;

		// 이 input 핀에 연결된 link 검색.
		uint32 SourcePinId = 0;
		for (const FAnimGraphLink& L : Graph.GetLinks())
		{
			if (L.ToPinId == InputPinId)
			{
				SourcePinId = L.FromPinId;
				break;
			}
		}
		if (SourcePinId == 0)
		{
			UE_LOG("AnimGraphCompiler: input pin %u 가 연결되지 않음 → RefPose fallback.", InputPinId);
			return Owner.MakeNode<FAnimNode_RefPose>();
		}

		// source 핀의 owning node.
		const FAnimGraphPin* SrcPin = Graph.FindPin(SourcePinId);
		if (!SrcPin)
		{
			UE_LOG("AnimGraphCompiler: dangling source pin id=%u", SourcePinId);
			return nullptr;
		}
		const FAnimGraphNode* SrcNode = Graph.FindNode(SrcPin->OwningNodeId);
		if (!SrcNode)
		{
			UE_LOG("AnimGraphCompiler: dangling owning node id=%u", SrcPin->OwningNodeId);
			return nullptr;
		}

		return CompileNode(Graph, Owner, *SrcNode);
	}

	FAnimNode_Base* CompileNode(const UAnimGraphAsset& Graph, UAnimInstance& Owner, const FAnimGraphNode& Node)
	{
		if (g_CompileDepth >= kMaxCompileDepth)
		{
			UE_LOG("AnimGraphCompiler: 재귀 깊이 한도 초과 (id=%u) — cycle 의심, RefPose fallback.", Node.NodeId);
			return Owner.MakeNode<FAnimNode_RefPose>();
		}
		FDepthScope DepthScope;

		switch (Node.Type)
		{
			case EAnimGraphNodeType::OutputPose:
			{
				// 종착점 — 단일 input 의 source 를 그대로 통과시키는 의미. ChildPose 자체를 반환.
				// (실제 FAnimNode_Root wrap 은 UAnimInstance::SetRootNode 가 자동으로 함.)
				for (const FAnimGraphPin& Pin : Node.Pins)
				{
					if (Pin.Kind == EAnimGraphPinKind::Input && Pin.Type == EAnimGraphPinType::Pose)
					{
						return CompileInputPose(Graph, Owner, Pin.PinId);
					}
				}
				UE_LOG("AnimGraphCompiler: OutputPose 노드에 Pose input 핀 없음.");
				return nullptr;
			}

			case EAnimGraphNodeType::SequencePlayer:
			{
				FAnimNode_SequencePlayer* SP = Owner.MakeNode<FAnimNode_SequencePlayer>();
				SP->Sequence = Node.SequenceRef;
				SP->PlayRate = Node.PlayRate;
				SP->bLooping = Node.bLooping;
				if (!Node.SequenceRef)
				{
					UE_LOG("AnimGraphCompiler: SequencePlayer 노드 id=%u 에 Sequence 미설정.", Node.NodeId);
				}
				return SP;
			}

			case EAnimGraphNodeType::Slot:
			{
				FAnimNode_Slot* SlotNode = Owner.MakeNode<FAnimNode_Slot>();
				// 비어있으면 DefaultMontageSlot fallback — 기존 UCharacterAnimInstance 패턴.
				SlotNode->SlotName = (Node.SlotName == FName::None)
					? UAnimInstance::DefaultMontageSlot
					: Node.SlotName;

				// Source input 의 source pin 따라 재귀 컴파일.
				if (const FAnimGraphPin* SrcIn = FindInputPinByName(Node, FName("Source")))
				{
					SlotNode->InputPose = CompileInputPose(Graph, Owner, SrcIn->PinId);
				}
				return SlotNode;
			}

			case EAnimGraphNodeType::LayeredBlendPerBone:
			{
				FAnimNode_LayeredBlendPerBone* LB = Owner.MakeNode<FAnimNode_LayeredBlendPerBone>();
				LB->BlendWeight = Node.BlendWeight;

				if (const FAnimGraphPin* BaseIn = FindInputPinByName(Node, FName("Base")))
				{
					LB->BasePose = CompileInputPose(Graph, Owner, BaseIn->PinId);
				}
				if (const FAnimGraphPin* BlendIn = FindInputPinByName(Node, FName("Blend")))
				{
					LB->BlendPose = CompileInputPose(Graph, Owner, BlendIn->PinId);
				}

				// PerBoneMask — RootBoneName 비어있으면 모든 본 true (full blend), 있으면
				// 그 본 + 자손 트리만 true (BuildBoneMaskFromRoot).
				if (USkeletalMeshComponent* Comp = Owner.GetOwningComponent())
				{
					if (USkeletalMesh* Mesh = Comp->GetSkeletalMesh())
					{
						if (!Node.RootBoneName.empty())
						{
							LB->PerBoneMask = BuildBoneMaskFromRoot(Mesh, Node.RootBoneName);
						}
						else if (FSkeletalMesh* MeshAsset = Mesh->GetSkeletalMeshAsset())
						{
							LB->PerBoneMask.assign(MeshAsset->Bones.size(), true);
						}
					}
				}
				return LB;
			}

			case EAnimGraphNodeType::BlendListByEnum:
			{
				FAnimNode_BlendListByEnum* Blend = Owner.MakeNode<FAnimNode_BlendListByEnum>();

				// "A" / "B" input → InputPoses[0..1]. 후속에 N 개로 확장 가능.
				for (const FName PinName : { FName("A"), FName("B") })
				{
					if (const FAnimGraphPin* InPin = FindInputPinByName(Node, PinName))
					{
						Blend->InputPoses.push_back(CompileInputPose(Graph, Owner, InPin->PinId));
					}
				}

				// "Selector" input source 가 VariableGet 노드면 reflection 람다 inline.
				// (int)floor + clamp [0, NumInputs) 로 InputPoses 인덱스 결정.
				if (const FAnimGraphPin* SelPin = FindInputPinByName(Node, FName("Selector")))
				{
					const FAnimGraphNode* SrcNode = FindInputSourceNode(Graph, SelPin->PinId);
					if (SrcNode && SrcNode->Type == EAnimGraphNodeType::VariableGet)
					{
						auto Reader = MakeFloatReader(SrcNode->VariableName);
						const int32 NumInputs = static_cast<int32>(Blend->InputPoses.size());
						Blend->SelectorFn = [Reader, NumInputs](UAnimInstance* AI) -> int32
						{
							const float V = Reader(AI);
							int32 Idx = static_cast<int32>(std::floor(V));
							if (Idx < 0) Idx = 0;
							if (Idx >= NumInputs) Idx = NumInputs - 1;
							return Idx;
						};
					}
				}
				return Blend;
			}

			case EAnimGraphNodeType::VariableGet:
			{
				// VariableGet 은 런타임 트리에 별도 노드로 박지 않음 — consumer (BlendListByEnum 등)
				// 가 source 메타데이터를 보고 람다로 inline. Graph 내 종착점 (Output 직결 등) 으로
				// 잘못 연결된 경우만 fallback.
				UE_LOG("AnimGraphCompiler: VariableGet 노드 id=%u 가 pose chain 에 연결됨 → RefPose fallback.", Node.NodeId);
				return Owner.MakeNode<FAnimNode_RefPose>();
			}

			case EAnimGraphNodeType::RefPose:
			{
				// 단순 leaf — mesh ref pose 출력. 보통 Slot/LayeredBlend 의 BlendPose 입력으로 사용
				// (montage 없는 UpperBody slot 패턴 등).
				return Owner.MakeNode<FAnimNode_RefPose>();
			}

			case EAnimGraphNodeType::StateMachine:
			{
				FAnimNode_StateMachine* SM = Owner.MakeNode<FAnimNode_StateMachine>();

				// 각 state — UAnimState UObject 생성.
				// SubGraphNodeId > 0 이면 그 노드 (StateMachine 가정) 를 재귀 컴파일해 SubGraphOverride.
				// 일반 sequence state 면 SequencePath 로드해 Sequence 박음.
				for (const FAnimGraphState& Def : Node.States)
				{
					UAnimState* S = UObjectManager::Get().CreateObject<UAnimState>(&Owner);
					S->StateName = Def.StateName;
					S->PlayRate  = Def.PlayRate;
					S->bLooping  = Def.bLooping;

					if (Def.SubGraphNodeId != 0 && Def.SubGraphNodeId != Node.NodeId)
					{
						const FAnimGraphNode* SubNode = Graph.FindNode(Def.SubGraphNodeId);
						if (SubNode && SubNode->Type == EAnimGraphNodeType::StateMachine)
						{
							S->SubGraphOverride = CompileNode(Graph, Owner, *SubNode);
						}
						else
						{
							UE_LOG("AnimGraphCompiler: State '%s' 의 SubGraphNodeId=%u 가 StateMachine 노드 아님.",
								Def.StateName.ToString().c_str(), Def.SubGraphNodeId);
						}
					}
					else if (!Def.SequencePath.empty() && Def.SequencePath != "None")
					{
						S->Sequence = FAnimationManager::Get().LoadAnimation(Def.SequencePath);
						if (!S->Sequence)
						{
							UE_LOG("AnimGraphCompiler: State '%s' 의 sequence 로드 실패. Path=%s",
								Def.StateName.ToString().c_str(), Def.SequencePath.c_str());
						}
					}
					SM->RegisterState(S);
				}

				// transitions — Var/Op/Threshold 비교식을 람다로 빌드.
				for (const FAnimGraphTransition& T : Node.Transitions)
				{
					FStateTransition Trans;
					Trans.From      = T.FromStateName;
					Trans.To        = T.ToStateName;
					Trans.BlendTime = T.BlendTime;
					Trans.Condition = MakeTransitionCondition(T.VariableName, T.Op, T.Threshold);
					SM->RegisterTransition(Trans);
				}

				SM->SetInitialState(Node.InitialStateName);
				return SM;
			}

			default:
				UE_LOG("AnimGraphCompiler: 미지원 노드 타입 (id=%u) → RefPose fallback.", Node.NodeId);
				return Owner.MakeNode<FAnimNode_RefPose>();
		}
	}
}

FAnimNode_Base* FAnimGraphCompiler::Compile(const UAnimGraphAsset& Graph, UAnimInstance& OwningInstance)
{
	// OutputPose 1개 — 0개 또는 다수면 실패.
	const FAnimGraphNode* OutputNode = nullptr;
	int32 OutputCount = 0;
	for (const FAnimGraphNode& N : Graph.GetNodes())
	{
		if (N.Type == EAnimGraphNodeType::OutputPose)
		{
			OutputNode = &N;
			++OutputCount;
		}
	}
	if (OutputCount != 1 || !OutputNode)
	{
		UE_LOG("AnimGraphCompiler: OutputPose 노드 개수 = %d (정확히 1 필요).", OutputCount);
		return nullptr;
	}

	return CompileNode(Graph, OwningInstance, *OutputNode);
}
