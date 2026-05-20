#include "AnimGraphCompiler.h"

#include "Animation/AnimGraphAsset.h"
#include "Animation/AnimGraphTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/Nodes/AnimNode_Base.h"
#include "Animation/Nodes/AnimNode_BlendListByEnum.h"
#include "Animation/Nodes/AnimNode_LayeredBlendPerBone.h"
#include "Animation/Nodes/AnimNode_RefPose.h"
#include "Animation/Nodes/AnimNode_SequencePlayer.h"
#include "Animation/Nodes/AnimNode_Slot.h"
#include "Component/SkeletalMeshComponent.h"
#include "Core/Log.h"
#include "Core/PropertyTypes.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Object/UClass.h"

#include <cmath>

namespace
{
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

				// PerBoneMask — F-1 한정 full blend (모든 본 true). 후속에 root bone 기반 부분 mask.
				if (USkeletalMeshComponent* Comp = Owner.GetOwningComponent())
				{
					if (USkeletalMesh* Mesh = Comp->GetSkeletalMesh())
					{
						if (FSkeletalMesh* MeshAsset = Mesh->GetSkeletalMeshAsset())
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

			// 후속 sub-step (F-3) 에서 추가.
			case EAnimGraphNodeType::StateMachine:
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
