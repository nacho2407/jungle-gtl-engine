#include "AnimGraphAsset.h"

#include "Serialization/Archive.h"

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

void UAnimGraphAsset::InitializeDefault()
{
	Nodes.clear();
	Links.clear();
	NextId = 1;

	// SequencePlayer (좌측) — output pose 1.
	FAnimGraphNode* Source = AddNode(EAnimGraphNodeType::SequencePlayer, FName("Sequence Player"), -240.0f, 0.0f);
	const uint32 SourceOut = AddPin(*Source, EAnimGraphPinKind::Output, EAnimGraphPinType::Pose, FName("Pose"))->PinId;

	// OutputPose (우측) — input pose 1. 그래프 종착점.
	FAnimGraphNode* Sink = AddNode(EAnimGraphNodeType::OutputPose, FName("Output Pose"), 0.0f, 0.0f);
	const uint32 SinkIn = AddPin(*Sink, EAnimGraphPinKind::Input, EAnimGraphPinType::Pose, FName("Result"))->PinId;

	AddLink(SourceOut, SinkIn);
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
