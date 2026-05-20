#include "AnimGraphCompiler.h"

#include "Animation/AnimGraphAsset.h"
#include "Animation/AnimGraphTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/Nodes/AnimNode_Base.h"
#include "Animation/Nodes/AnimNode_RefPose.h"
#include "Animation/Nodes/AnimNode_SequencePlayer.h"
#include "Core/Log.h"

namespace
{
	// graph 의 한 노드 → FAnimNode_* 인스턴스. 재귀 — 입력 핀의 source 를 따라 자식 build.
	// nullptr 반환은 컴파일 실패 (호출 chain 어디선가 미지원 / dangling).
	FAnimNode_Base* CompileNode(const UAnimGraphAsset& Graph, UAnimInstance& Owner, const FAnimGraphNode& Node);

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

			// 단계 D1 미지원 — 후속 단계에서 각 노드 타입 컴파일러 추가.
			case EAnimGraphNodeType::StateMachine:
			case EAnimGraphNodeType::Slot:
			case EAnimGraphNodeType::LayeredBlendPerBone:
			case EAnimGraphNodeType::BlendListByEnum:
			case EAnimGraphNodeType::VariableGet:
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
