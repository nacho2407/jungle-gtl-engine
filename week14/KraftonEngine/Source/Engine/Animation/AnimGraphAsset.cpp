#include "AnimGraphAsset.h"

#include "Serialization/Archive.h"

#include <algorithm>

// ── AnimGraphTypes operator<< ──
//
// 각 struct 가 FName / TArray 같은 non-trivially-copyable 멤버를 보유 — FArchive 의 default
// trivially_copyable 경로를 못 타므로 element-wise 직렬화를 직접 작성.

FArchive& operator<<(FArchive& Ar, FAnimGraphPin& Pin)
{
	Ar << Pin.PinId;
	Ar << Pin.OwningNodeId;
	Ar << Pin.Kind;        // enum class : uint8 — trivially_copyable
	Ar << Pin.Type;
	Ar << Pin.DisplayName;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FAnimGraphLink& Link)
{
	Ar << Link.LinkId;
	Ar << Link.FromPinId;
	Ar << Link.ToPinId;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FAnimGraphNode& Node)
{
	Ar << Node.NodeId;
	Ar << Node.Type;
	Ar << Node.DisplayName;
	Ar << Node.PosX;
	Ar << Node.PosY;
	Ar << Node.Pins;
	Ar << Node.PlayRate;
	Ar << Node.bLooping;
	Ar << Node.SequencePath; // SequenceRef 는 transient — Initialize 시 path 로 재해상.
	return Ar;
}

// ── UAnimGraphAsset ──

FAnimGraphNode* UAnimGraphAsset::AddNode(EAnimGraphNodeType Type, const FName& DisplayName, float X, float Y)
{
	FAnimGraphNode Node;
	Node.NodeId      = AllocateId();
	Node.Type        = Type;
	Node.DisplayName = DisplayName;
	Node.PosX        = X;
	Node.PosY        = Y;
	Nodes.push_back(std::move(Node));
	return &Nodes.back();
}

FAnimGraphPin* UAnimGraphAsset::AddPin(FAnimGraphNode& Node, EAnimGraphPinKind Kind, EAnimGraphPinType PinType, const FName& DisplayName)
{
	FAnimGraphPin Pin;
	Pin.PinId        = AllocateId();
	Pin.OwningNodeId = Node.NodeId;
	Pin.Kind         = Kind;
	Pin.Type         = PinType;
	Pin.DisplayName  = DisplayName;
	Node.Pins.push_back(std::move(Pin));
	return &Node.Pins.back();
}

FAnimGraphLink* UAnimGraphAsset::AddLink(uint32 FromPinId, uint32 ToPinId)
{
	FAnimGraphLink Link;
	Link.LinkId    = AllocateId();
	Link.FromPinId = FromPinId;
	Link.ToPinId   = ToPinId;
	Links.push_back(std::move(Link));
	return &Links.back();
}

FAnimGraphNode* UAnimGraphAsset::AddNodeOfType(EAnimGraphNodeType Type, float X, float Y)
{
	// 타입별 default display 이름 + 핀 레이아웃. 후속 단계의 컴파일러가 같은 핀 이름을
	// 키로 트리 build 에 사용하므로 핀 명세는 stable.
	switch (Type)
	{
		case EAnimGraphNodeType::OutputPose:
		{
			FAnimGraphNode* N = AddNode(Type, FName("Output Pose"), X, Y);
			AddPin(*N, EAnimGraphPinKind::Input, EAnimGraphPinType::Pose, FName("Result"));
			return N;
		}
		case EAnimGraphNodeType::SequencePlayer:
		{
			FAnimGraphNode* N = AddNode(Type, FName("Sequence Player"), X, Y);
			AddPin(*N, EAnimGraphPinKind::Output, EAnimGraphPinType::Pose, FName("Pose"));
			return N;
		}
		case EAnimGraphNodeType::StateMachine:
		{
			FAnimGraphNode* N = AddNode(Type, FName("State Machine"), X, Y);
			AddPin(*N, EAnimGraphPinKind::Output, EAnimGraphPinType::Pose, FName("Pose"));
			return N;
		}
		case EAnimGraphNodeType::Slot:
		{
			FAnimGraphNode* N = AddNode(Type, FName("Slot"), X, Y);
			AddPin(*N, EAnimGraphPinKind::Input,  EAnimGraphPinType::Pose, FName("Source"));
			AddPin(*N, EAnimGraphPinKind::Output, EAnimGraphPinType::Pose, FName("Result"));
			return N;
		}
		case EAnimGraphNodeType::LayeredBlendPerBone:
		{
			FAnimGraphNode* N = AddNode(Type, FName("Layered Blend"), X, Y);
			AddPin(*N, EAnimGraphPinKind::Input,  EAnimGraphPinType::Pose, FName("Base"));
			AddPin(*N, EAnimGraphPinKind::Input,  EAnimGraphPinType::Pose, FName("Blend"));
			AddPin(*N, EAnimGraphPinKind::Output, EAnimGraphPinType::Pose, FName("Result"));
			return N;
		}
		case EAnimGraphNodeType::BlendListByEnum:
		{
			FAnimGraphNode* N = AddNode(Type, FName("Blend List By Enum"), X, Y);
			AddPin(*N, EAnimGraphPinKind::Input,  EAnimGraphPinType::Int,  FName("Selector"));
			AddPin(*N, EAnimGraphPinKind::Input,  EAnimGraphPinType::Pose, FName("A"));
			AddPin(*N, EAnimGraphPinKind::Input,  EAnimGraphPinType::Pose, FName("B"));
			AddPin(*N, EAnimGraphPinKind::Output, EAnimGraphPinType::Pose, FName("Result"));
			return N;
		}
		case EAnimGraphNodeType::VariableGet:
		{
			FAnimGraphNode* N = AddNode(Type, FName("Variable"), X, Y);
			AddPin(*N, EAnimGraphPinKind::Output, EAnimGraphPinType::Float, FName("Value"));
			return N;
		}
	}
	return nullptr;
}

bool UAnimGraphAsset::RemoveNode(uint32 NodeId)
{
	if (NodeId == 0) return false;

	// 노드의 핀 id 들을 먼저 수집 → 그 핀이 from/to 에 사용된 모든 링크 cascade 제거.
	TArray<uint32> PinIds;
	for (const FAnimGraphNode& Node : Nodes)
	{
		if (Node.NodeId != NodeId) continue;
		PinIds.reserve(Node.Pins.size());
		for (const FAnimGraphPin& Pin : Node.Pins) PinIds.push_back(Pin.PinId);
		break;
	}

	if (PinIds.empty())
	{
		// 노드를 못 찾았으면 아무것도 안 함.
		const bool bFound = std::any_of(Nodes.begin(), Nodes.end(),
			[NodeId](const FAnimGraphNode& N) { return N.NodeId == NodeId; });
		if (!bFound) return false;
	}

	Links.erase(std::remove_if(Links.begin(), Links.end(),
		[&PinIds](const FAnimGraphLink& L)
		{
			for (uint32 P : PinIds)
			{
				if (L.FromPinId == P || L.ToPinId == P) return true;
			}
			return false;
		}), Links.end());

	Nodes.erase(std::remove_if(Nodes.begin(), Nodes.end(),
		[NodeId](const FAnimGraphNode& N) { return N.NodeId == NodeId; }), Nodes.end());

	return true;
}

bool UAnimGraphAsset::RemoveLink(uint32 LinkId)
{
	if (LinkId == 0) return false;
	const size_t Before = Links.size();
	Links.erase(std::remove_if(Links.begin(), Links.end(),
		[LinkId](const FAnimGraphLink& L) { return L.LinkId == LinkId; }), Links.end());
	return Links.size() != Before;
}

bool UAnimGraphAsset::CanLinkPins(uint32 PinAId, uint32 PinBId, uint32* OutFromPinId, uint32* OutToPinId) const
{
	if (PinAId == 0 || PinBId == 0 || PinAId == PinBId) return false;

	const FAnimGraphPin* A = FindPin(PinAId);
	const FAnimGraphPin* B = FindPin(PinBId);
	if (!A || !B) return false;

	// 같은 노드 내 핀 끼리 연결 금지.
	if (A->OwningNodeId == B->OwningNodeId) return false;

	// Kind 가 반대여야 함 — 양쪽 다 input 또는 양쪽 다 output 이면 거부.
	if (A->Kind == B->Kind) return false;

	// 타입 일치 — Pose-Pose, Float-Float 등.
	if (A->Type != B->Type) return false;

	// 방향 정규화 — 드래그 방향에 무관하게 from=Output, to=Input.
	const FAnimGraphPin* From = (A->Kind == EAnimGraphPinKind::Output) ? A : B;
	const FAnimGraphPin* To   = (From == A) ? B : A;

	// 중복 링크 거부 — UE 도 동일 (1 input 에 1 output 의 multi-fanout 은 허용, 같은 from→to 중복은 금지).
	for (const FAnimGraphLink& L : Links)
	{
		if (L.FromPinId == From->PinId && L.ToPinId == To->PinId) return false;
	}

	if (OutFromPinId) *OutFromPinId = From->PinId;
	if (OutToPinId)   *OutToPinId   = To->PinId;
	return true;
}

bool UAnimGraphAsset::HasOutputPoseNode() const
{
	for (const FAnimGraphNode& N : Nodes)
	{
		if (N.Type == EAnimGraphNodeType::OutputPose) return true;
	}
	return false;
}

void UAnimGraphAsset::InitializeDefault()
{
	Nodes.clear();
	Links.clear();
	NextId = 1;

	// ⚠ Nodes 가 std::vector — 후속 AddNode 호출이 reallocation 을 일으키면 이전에 받은
	// raw pointer 가 invalidate 됨. id 만 즉시 캡쳐하고 pointer 는 버린다.
	uint32 SourceOutId = 0;
	if (FAnimGraphNode* Source = AddNodeOfType(EAnimGraphNodeType::SequencePlayer, -240.0f, 0.0f))
	{
		if (!Source->Pins.empty()) SourceOutId = Source->Pins.front().PinId;
	}

	uint32 SinkInId = 0;
	if (FAnimGraphNode* Sink = AddNodeOfType(EAnimGraphNodeType::OutputPose, 0.0f, 0.0f))
	{
		if (!Sink->Pins.empty()) SinkInId = Sink->Pins.front().PinId;
	}

	if (SourceOutId && SinkInId)
	{
		AddLink(SourceOutId, SinkInId);
	}
}

FAnimGraphNode* UAnimGraphAsset::FindNode(uint32 NodeId)
{
	if (NodeId == 0) return nullptr;
	for (FAnimGraphNode& Node : Nodes)
	{
		if (Node.NodeId == NodeId) return &Node;
	}
	return nullptr;
}

const FAnimGraphNode* UAnimGraphAsset::FindNode(uint32 NodeId) const
{
	if (NodeId == 0) return nullptr;
	for (const FAnimGraphNode& Node : Nodes)
	{
		if (Node.NodeId == NodeId) return &Node;
	}
	return nullptr;
}

FAnimGraphNode* UAnimGraphAsset::FindFirstNodeOfType(EAnimGraphNodeType Type)
{
	for (FAnimGraphNode& Node : Nodes)
	{
		if (Node.Type == Type) return &Node;
	}
	return nullptr;
}

FAnimGraphPin* UAnimGraphAsset::FindPin(uint32 PinId)
{
	if (PinId == 0) return nullptr;
	for (FAnimGraphNode& Node : Nodes)
	{
		for (FAnimGraphPin& Pin : Node.Pins)
		{
			if (Pin.PinId == PinId) return &Pin;
		}
	}
	return nullptr;
}

const FAnimGraphPin* UAnimGraphAsset::FindPin(uint32 PinId) const
{
	if (PinId == 0) return nullptr;
	for (const FAnimGraphNode& Node : Nodes)
	{
		for (const FAnimGraphPin& Pin : Node.Pins)
		{
			if (Pin.PinId == PinId) return &Pin;
		}
	}
	return nullptr;
}

void UAnimGraphAsset::Serialize(FArchive& Ar)
{
	// UObject::Serialize 호출 안 함 — 다른 자산(UFloatCurveAsset 등)과 동일 패턴 (자산 패키지
	// 컨텍스트에서 ObjectName 직렬화 불필요).
	Ar << NextId;
	Ar << Nodes;
	Ar << Links;
}
