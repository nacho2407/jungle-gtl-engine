#include "Materials/Graph/MaterialGraphAsset.h"

#include <algorithm>
#include <cstdio>

namespace
{
	float JsonNumberToFloat(const json::JSON& Value, float Default = 0.0f)
	{
		if (Value.JSONType() == json::JSON::Class::Floating) return static_cast<float>(Value.ToFloat());
		if (Value.JSONType() == json::JSON::Class::Integral) return static_cast<float>(Value.ToInt());
		return Default;
	}

	uint32 JsonNumberToU32(const json::JSON& Value, uint32 Default = 0)
	{
		if (Value.JSONType() == json::JSON::Class::Integral) return static_cast<uint32>(Value.ToInt());
		if (Value.JSONType() == json::JSON::Class::Floating) return static_cast<uint32>(Value.ToFloat());
		return Default;
	}

	const json::JSON& JsonMember(const json::JSON& Object, const char* Key)
	{
		static const json::JSON NullJson;
		if (Object.JSONType() == json::JSON::Class::Object && Object.hasKey(Key))
		{
			return Object.at(Key);
		}
		return NullJson;
	}

	json::JSON VectorToJson(const FVector4& Value)
	{
		return json::Array(Value.X, Value.Y, Value.Z, Value.W);
	}

	FVector4 VectorFromJson(const json::JSON& Value, const FVector4& Default)
	{
		if (Value.JSONType() != json::JSON::Class::Array)
		{
			return Default;
		}

		FVector4 Out = Default;
		if (Value.length() > 0) Out.X = JsonNumberToFloat(Value.at(0), Out.X);
		if (Value.length() > 1) Out.Y = JsonNumberToFloat(Value.at(1), Out.Y);
		if (Value.length() > 2) Out.Z = JsonNumberToFloat(Value.at(2), Out.Z);
		if (Value.length() > 3) Out.W = JsonNumberToFloat(Value.at(3), Out.W);
		return Out;
	}

	void SavePin(const FMaterialGraphPin& Pin, json::JSON& Out)
	{
		Out["PinId"] = static_cast<int>(Pin.PinId);
		Out["OwningNodeId"] = static_cast<int>(Pin.OwningNodeId);
		Out["Kind"] = Pin.Kind == EMaterialGraphPinKind::Input ? "Input" : "Output";
		Out["Type"] = ToString(Pin.Type);
		Out["DisplayName"] = Pin.DisplayName.ToString();
	}

	FMaterialGraphPin LoadPin(const json::JSON& Json)
	{
		FMaterialGraphPin Pin;
		Pin.PinId = JsonNumberToU32(JsonMember(Json, "PinId"));
		Pin.OwningNodeId = JsonNumberToU32(JsonMember(Json, "OwningNodeId"));
		Pin.Kind = JsonMember(Json, "Kind").ToString() == "Output" ? EMaterialGraphPinKind::Output : EMaterialGraphPinKind::Input;
		Pin.Type = MaterialPinTypeFromString(JsonMember(Json, "Type").ToString(), EMaterialGraphPinType::Float);
		Pin.DisplayName = FName(JsonMember(Json, "DisplayName").ToString());
		return Pin;
	}
}

FMaterialGraphNode* FMaterialGraph::AddNode(EMaterialGraphNodeType Type, const FName& DisplayName, float X, float Y)
{
	FMaterialGraphNode Node;
	Node.NodeId = AllocateId();
	Node.Type = Type;
	Node.DisplayName = DisplayName;
	Node.PosX = X;
	Node.PosY = Y;
	Nodes.push_back(std::move(Node));
	return &Nodes.back();
}

FMaterialGraphPin* FMaterialGraph::AddPin(FMaterialGraphNode& Node, EMaterialGraphPinKind Kind, EMaterialGraphPinType PinType, const FName& DisplayName)
{
	FMaterialGraphPin Pin;
	Pin.PinId = AllocateId();
	Pin.OwningNodeId = Node.NodeId;
	Pin.Kind = Kind;
	Pin.Type = PinType;
	Pin.DisplayName = DisplayName;
	Node.Pins.push_back(std::move(Pin));
	return &Node.Pins.back();
}

FMaterialGraphLink* FMaterialGraph::AddLink(uint32 FromPinId, uint32 ToPinId)
{
	FMaterialGraphLink Link;
	Link.LinkId = AllocateId();
	Link.FromPinId = FromPinId;
	Link.ToPinId = ToPinId;
	Links.push_back(std::move(Link));
	return &Links.back();
}

FMaterialGraphNode* FMaterialGraph::AddNodeOfType(EMaterialGraphNodeType Type, float X, float Y, EMaterialDomain Domain)
{
	switch (Type)
	{
	case EMaterialGraphNodeType::Output:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Material Output"), X, Y);
		RebuildOutputPinsForDomain(Domain);
		return N;
	}
	case EMaterialGraphNodeType::TextureObject:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Texture Object"), X, Y);
		N->ParameterName = "Diffuse";
		N->TextureSlot = EMaterialTextureSlot::Diffuse;
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Texture2D, FName("Texture"));
		return N;
	}
	case EMaterialGraphNodeType::TextureSample:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Texture Sample"), X, Y);
		AddPin(*N, EMaterialGraphPinKind::Input, EMaterialGraphPinType::Texture2D, FName("Texture"));
		AddPin(*N, EMaterialGraphPinKind::Input, EMaterialGraphPinType::Float2, FName("UV"));
		// 채널별 분리 출력 + 풀 출력. InputExpr이 pin 이름으로 swizzle 처리.
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float3, FName("RGB"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float,  FName("R"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float,  FName("G"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float,  FName("B"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float,  FName("A"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float4, FName("RGBA"));
		return N;
	}
	case EMaterialGraphNodeType::ScalarParameter:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Scalar Parameter"), X, Y);
		N->ParameterName = "Scalar";
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float, FName("Value"));
		return N;
	}
	case EMaterialGraphNodeType::VectorParameter:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Vector Parameter"), X, Y);
		N->ParameterName = "Vector";
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float4, FName("Value"));
		return N;
	}
	case EMaterialGraphNodeType::ColorParameter:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Color Parameter"), X, Y);
		N->ParameterName = "Color";
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Color, FName("Color"));
		return N;
	}
	case EMaterialGraphNodeType::ConstantFloat:
	case EMaterialGraphNodeType::ConstantFloat2:
	case EMaterialGraphNodeType::ConstantFloat3:
	case EMaterialGraphNodeType::ConstantFloat4:
	{
		const EMaterialGraphPinType PinType =
			Type == EMaterialGraphNodeType::ConstantFloat ? EMaterialGraphPinType::Float :
			Type == EMaterialGraphNodeType::ConstantFloat2 ? EMaterialGraphPinType::Float2 :
			Type == EMaterialGraphNodeType::ConstantFloat3 ? EMaterialGraphPinType::Float3 :
			EMaterialGraphPinType::Float4;
		FMaterialGraphNode* N = AddNode(Type, FName(ToString(Type)), X, Y);
		AddPin(*N, EMaterialGraphPinKind::Output, PinType, FName("Value"));
		return N;
	}
	case EMaterialGraphNodeType::Add:
	case EMaterialGraphNodeType::Subtract:
	case EMaterialGraphNodeType::Multiply:
	case EMaterialGraphNodeType::Divide:
	case EMaterialGraphNodeType::Power:
	{
		FMaterialGraphNode* N = AddNode(Type, FName(ToString(Type)), X, Y);
		AddPin(*N, EMaterialGraphPinKind::Input, EMaterialGraphPinType::Float4, FName("A"));
		AddPin(*N, EMaterialGraphPinKind::Input, EMaterialGraphPinType::Float4, FName("B"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float4, FName("Result"));
		return N;
	}
	case EMaterialGraphNodeType::OneMinus:
	case EMaterialGraphNodeType::Saturate:
	{
		FMaterialGraphNode* N = AddNode(Type, FName(ToString(Type)), X, Y);
		AddPin(*N, EMaterialGraphPinKind::Input, EMaterialGraphPinType::Float4, FName("Value"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float4, FName("Result"));
		return N;
	}
	case EMaterialGraphNodeType::Clamp:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Clamp"), X, Y);
		AddPin(*N, EMaterialGraphPinKind::Input, EMaterialGraphPinType::Float4, FName("Value"));
		AddPin(*N, EMaterialGraphPinKind::Input, EMaterialGraphPinType::Float4, FName("Min"));
		AddPin(*N, EMaterialGraphPinKind::Input, EMaterialGraphPinType::Float4, FName("Max"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float4, FName("Result"));
		return N;
	}
	case EMaterialGraphNodeType::Lerp:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Lerp"), X, Y);
		AddPin(*N, EMaterialGraphPinKind::Input, EMaterialGraphPinType::Float4, FName("A"));
		AddPin(*N, EMaterialGraphPinKind::Input, EMaterialGraphPinType::Float4, FName("B"));
		AddPin(*N, EMaterialGraphPinKind::Input, EMaterialGraphPinType::Float, FName("Alpha"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float4, FName("Result"));
		return N;
	}
	case EMaterialGraphNodeType::TexCoord:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("TexCoord"), X, Y);
		// Value.X = UV channel index (0/1/2). 기본 0.
		N->Value = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float2, FName("UV"));
		return N;
	}
	case EMaterialGraphNodeType::Panner:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Panner"), X, Y);
		N->Value = FVector4(0.1f, 0.0f, 0.0f, 0.0f);
		AddPin(*N, EMaterialGraphPinKind::Input, EMaterialGraphPinType::Float2, FName("UV"));
		AddPin(*N, EMaterialGraphPinKind::Input, EMaterialGraphPinType::Float, FName("Time"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float2, FName("UV"));
		return N;
	}
	case EMaterialGraphNodeType::Time:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Time"), X, Y);
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float, FName("Time"));
		return N;
	}
	case EMaterialGraphNodeType::VertexColor:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Vertex Color"), X, Y);
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float3, FName("RGB"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float,  FName("R"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float,  FName("G"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float,  FName("B"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float,  FName("A"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float4, FName("RGBA"));
		return N;
	}
	case EMaterialGraphNodeType::ParticleColor:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Particle Color"), X, Y);
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float3, FName("RGB"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float,  FName("R"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float,  FName("G"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float,  FName("B"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float,  FName("A"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float4, FName("RGBA"));
		return N;
	}
	case EMaterialGraphNodeType::Append:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Append"), X, Y);
		AddPin(*N, EMaterialGraphPinKind::Input, EMaterialGraphPinType::Float, FName("A"));
		AddPin(*N, EMaterialGraphPinKind::Input, EMaterialGraphPinType::Float, FName("B"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float2, FName("Result"));
		return N;
	}
	case EMaterialGraphNodeType::ComponentMask:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Component Mask"), X, Y);
		N->Mask = "RGB";
		AddPin(*N, EMaterialGraphPinKind::Input, EMaterialGraphPinType::Float4, FName("Value"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float3, FName("Result"));
		return N;
	}
	case EMaterialGraphNodeType::ConstantBiasScale:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("ConstantBiasScale"), X, Y);
		// Value.X = Bias, Value.Y = Scale. UE 기본값 (-1,1 remap) 흉내.
		N->Value = FVector4(-0.5f, 2.0f, 0.0f, 0.0f);
		AddPin(*N, EMaterialGraphPinKind::Input,  EMaterialGraphPinType::Float, FName("Value"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float, FName("Result"));
		return N;
	}
	case EMaterialGraphNodeType::Distance:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Distance"), X, Y);
		AddPin(*N, EMaterialGraphPinKind::Input,  EMaterialGraphPinType::Float3, FName("A"));
		AddPin(*N, EMaterialGraphPinKind::Input,  EMaterialGraphPinType::Float3, FName("B"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float,  FName("Result"));
		return N;
	}
	case EMaterialGraphNodeType::Normalize:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Normalize"), X, Y);
		AddPin(*N, EMaterialGraphPinKind::Input,  EMaterialGraphPinType::Float3, FName("Value"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float3, FName("Result"));
		return N;
	}
	case EMaterialGraphNodeType::Dot:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Dot"), X, Y);
		AddPin(*N, EMaterialGraphPinKind::Input,  EMaterialGraphPinType::Float3, FName("A"));
		AddPin(*N, EMaterialGraphPinKind::Input,  EMaterialGraphPinType::Float3, FName("B"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float,  FName("Result"));
		return N;
	}
	case EMaterialGraphNodeType::Cross:
	{
		FMaterialGraphNode* N = AddNode(Type, FName("Cross"), X, Y);
		AddPin(*N, EMaterialGraphPinKind::Input,  EMaterialGraphPinType::Float3, FName("A"));
		AddPin(*N, EMaterialGraphPinKind::Input,  EMaterialGraphPinType::Float3, FName("B"));
		AddPin(*N, EMaterialGraphPinKind::Output, EMaterialGraphPinType::Float3, FName("Result"));
		return N;
	}
	}
	return nullptr;
}

bool FMaterialGraph::RemoveNode(uint32 NodeId)
{
	if (NodeId == 0) return false;

	TArray<uint32> PinIds;
	for (const FMaterialGraphNode& Node : Nodes)
	{
		if (Node.NodeId != NodeId) continue;
		for (const FMaterialGraphPin& Pin : Node.Pins) PinIds.push_back(Pin.PinId);
		break;
	}

	Links.erase(std::remove_if(Links.begin(), Links.end(),
		[&PinIds](const FMaterialGraphLink& L)
		{
			for (uint32 PinId : PinIds)
			{
				if (L.FromPinId == PinId || L.ToPinId == PinId) return true;
			}
			return false;
		}), Links.end());

	const size_t Before = Nodes.size();
	Nodes.erase(std::remove_if(Nodes.begin(), Nodes.end(),
		[NodeId](const FMaterialGraphNode& N) { return N.NodeId == NodeId; }), Nodes.end());
	return Nodes.size() != Before;
}

bool FMaterialGraph::RemoveLink(uint32 LinkId)
{
	if (LinkId == 0) return false;
	const size_t Before = Links.size();
	Links.erase(std::remove_if(Links.begin(), Links.end(),
		[LinkId](const FMaterialGraphLink& L) { return L.LinkId == LinkId; }), Links.end());
	return Links.size() != Before;
}

bool FMaterialGraph::CanLinkPins(uint32 PinAId, uint32 PinBId, uint32* OutFromPinId, uint32* OutToPinId) const
{
	if (PinAId == 0 || PinBId == 0 || PinAId == PinBId) return false;

	const FMaterialGraphPin* A = FindPin(PinAId);
	const FMaterialGraphPin* B = FindPin(PinBId);
	if (!A || !B) return false;
	if (A->OwningNodeId == B->OwningNodeId) return false;
	if (A->Kind == B->Kind) return false;

	const FMaterialGraphPin* From = A->Kind == EMaterialGraphPinKind::Output ? A : B;
	const FMaterialGraphPin* To = From == A ? B : A;
	if (!IsMaterialGraphPinTypeConvertible(From->Type, To->Type)) return false;

	for (const FMaterialGraphLink& L : Links)
	{
		if (L.ToPinId == To->PinId) return false;
		if (L.FromPinId == From->PinId && L.ToPinId == To->PinId) return false;
	}

	if (OutFromPinId) *OutFromPinId = From->PinId;
	if (OutToPinId) *OutToPinId = To->PinId;
	return true;
}

bool FMaterialGraph::HasOutputNode() const
{
	return FindFirstNodeOfType(EMaterialGraphNodeType::Output) != nullptr;
}

FMaterialGraphNode* FMaterialGraph::FindNode(uint32 NodeId)
{
	for (FMaterialGraphNode& Node : Nodes)
	{
		if (Node.NodeId == NodeId) return &Node;
	}
	return nullptr;
}

const FMaterialGraphNode* FMaterialGraph::FindNode(uint32 NodeId) const
{
	for (const FMaterialGraphNode& Node : Nodes)
	{
		if (Node.NodeId == NodeId) return &Node;
	}
	return nullptr;
}

FMaterialGraphPin* FMaterialGraph::FindPin(uint32 PinId)
{
	for (FMaterialGraphNode& Node : Nodes)
	{
		for (FMaterialGraphPin& Pin : Node.Pins)
		{
			if (Pin.PinId == PinId) return &Pin;
		}
	}
	return nullptr;
}

const FMaterialGraphPin* FMaterialGraph::FindPin(uint32 PinId) const
{
	for (const FMaterialGraphNode& Node : Nodes)
	{
		for (const FMaterialGraphPin& Pin : Node.Pins)
		{
			if (Pin.PinId == PinId) return &Pin;
		}
	}
	return nullptr;
}

FMaterialGraphNode* FMaterialGraph::FindFirstNodeOfType(EMaterialGraphNodeType Type)
{
	for (FMaterialGraphNode& Node : Nodes)
	{
		if (Node.Type == Type) return &Node;
	}
	return nullptr;
}

const FMaterialGraphNode* FMaterialGraph::FindFirstNodeOfType(EMaterialGraphNodeType Type) const
{
	for (const FMaterialGraphNode& Node : Nodes)
	{
		if (Node.Type == Type) return &Node;
	}
	return nullptr;
}

void FMaterialGraph::InitializeDefault(EMaterialDomain Domain)
{
	Nodes.clear();
	Links.clear();
	NextId = 1;

	// 파티클 도메인은 텍스처가 없어도 보이도록 ParticleColor → Color/Opacity 만 연결.
	// 사용자가 텍스처를 추가하면 TextureSample을 끼워 넣어 곱하면 됨.
	if (Domain == EMaterialDomain::ParticleSprite || Domain == EMaterialDomain::ParticleMesh)
	{
		uint32 RGBOutPin = 0, AOutPin = 0;
		if (FMaterialGraphNode* N = AddNodeOfType(EMaterialGraphNodeType::ParticleColor, -240.0f, 80.0f, Domain))
		{
			for (const FMaterialGraphPin& P : N->Pins)
			{
				if (P.Kind != EMaterialGraphPinKind::Output) continue;
				const FString PN = P.DisplayName.ToString();
				if (PN == "RGB") RGBOutPin = P.PinId;
				else if (PN == "A") AOutPin = P.PinId;
			}
		}

		FMaterialGraphNode* Out = AddNodeOfType(EMaterialGraphNodeType::Output, 80.0f, 80.0f, Domain);

		for (const FMaterialGraphPin& Pin : Out->Pins)
		{
			const FString PinName = Pin.DisplayName.ToString();
			if (PinName == "Color")        AddLink(RGBOutPin, Pin.PinId);
			else if (PinName == "Opacity") AddLink(AOutPin,   Pin.PinId);
		}
		return;
	}

	// Surface/Decal/PostProcess 등 — 텍스처 기반 기본 그래프.
	uint32 TexOut = 0;
	if (FMaterialGraphNode* N = AddNodeOfType(EMaterialGraphNodeType::TextureObject, -720.0f, -80.0f, Domain))
		TexOut = N->Pins[0].PinId;

	uint32 UVOut = 0;
	if (FMaterialGraphNode* N = AddNodeOfType(EMaterialGraphNodeType::TexCoord, -720.0f, 120.0f, Domain))
		UVOut = N->Pins[0].PinId;

	uint32 SampleTexIn = 0, SampleUVIn = 0, SampleOut = 0;
	if (FMaterialGraphNode* N = AddNodeOfType(EMaterialGraphNodeType::TextureSample, -480.0f, 0.0f, Domain))
	{
		SampleTexIn = N->Pins[0].PinId;
		SampleUVIn = N->Pins[1].PinId;
		SampleOut = N->Pins[2].PinId;
	}

	uint32 VertexColorOut = 0;
	if (FMaterialGraphNode* N = AddNodeOfType(EMaterialGraphNodeType::VertexColor, -480.0f, 220.0f, Domain))
		VertexColorOut = N->Pins[0].PinId;

	uint32 MulAIn = 0, MulBIn = 0, MulOut = 0;
	if (FMaterialGraphNode* N = AddNodeOfType(EMaterialGraphNodeType::Multiply, -240.0f, 80.0f, Domain))
	{
		MulAIn = N->Pins[0].PinId;
		MulBIn = N->Pins[1].PinId;
		MulOut = N->Pins[2].PinId;
	}

	uint32 RGBIn = 0, RGBOut = 0;
	if (FMaterialGraphNode* N = AddNodeOfType(EMaterialGraphNodeType::ComponentMask, 0.0f, 0.0f, Domain))
	{
		RGBIn = N->Pins[0].PinId;
		RGBOut = N->Pins[1].PinId;
	}

	uint32 AIn = 0, AOut = 0;
	if (FMaterialGraphNode* N = AddNodeOfType(EMaterialGraphNodeType::ComponentMask, 0.0f, 180.0f, Domain))
	{
		N->Mask = "A";
		if (N->Pins.size() >= 2) N->Pins[1].Type = EMaterialGraphPinType::Float;
		AIn = N->Pins[0].PinId;
		AOut = N->Pins[1].PinId;
	}

	FMaterialGraphNode* Out = AddNodeOfType(EMaterialGraphNodeType::Output, 260.0f, 80.0f, Domain);

	AddLink(TexOut, SampleTexIn);
	AddLink(UVOut, SampleUVIn);
	AddLink(SampleOut, MulAIn);
	AddLink(VertexColorOut, MulBIn);
	AddLink(MulOut, RGBIn);
	AddLink(MulOut, AIn);

	for (const FMaterialGraphPin& Pin : Out->Pins)
	{
		const FString PinName = Pin.DisplayName.ToString();
		if (PinName == "Color" || PinName == "BaseColor")
		{
			AddLink(RGBOut, Pin.PinId);
		}
		else if (PinName == "Opacity")
		{
			AddLink(AOut, Pin.PinId);
		}
	}
}

void FMaterialGraph::ApplyTexturedParticlePreset(EMaterialDomain Domain)
{
	Nodes.clear();
	Links.clear();
	NextId = 1;

	// ⚠ 노드 포인터를 AddNodeOfType 호출 사이에 유지하면 안 됨 — push_back이 재할당하면 dangling.
	//     각 if-block 안에서 PinId만 즉시 캡처하고 포인터는 버린다. Out 노드만 마지막에 추가되어 안전.

	uint32 TexOut = 0;
	if (FMaterialGraphNode* N = AddNodeOfType(EMaterialGraphNodeType::TextureObject, -720.0f, -80.0f, Domain))
	{
		TexOut = N->Pins[0].PinId;
	}

	uint32 SampleTexIn = 0, SampleRGB = 0, SampleA = 0;
	if (FMaterialGraphNode* N = AddNodeOfType(EMaterialGraphNodeType::TextureSample, -480.0f, 0.0f, Domain))
	{
		for (const FMaterialGraphPin& P : N->Pins)
		{
			const FString PN = P.DisplayName.ToString();
			if (P.Kind == EMaterialGraphPinKind::Input  && PN == "Texture") SampleTexIn = P.PinId;
			else if (P.Kind == EMaterialGraphPinKind::Output && PN == "RGB") SampleRGB = P.PinId;
			else if (P.Kind == EMaterialGraphPinKind::Output && PN == "A")   SampleA   = P.PinId;
		}
	}

	uint32 PColRGB = 0, PColA = 0;
	if (FMaterialGraphNode* N = AddNodeOfType(EMaterialGraphNodeType::ParticleColor, -480.0f, 220.0f, Domain))
	{
		for (const FMaterialGraphPin& P : N->Pins)
		{
			if (P.Kind != EMaterialGraphPinKind::Output) continue;
			const FString PN = P.DisplayName.ToString();
			if (PN == "RGB")      PColRGB = P.PinId;
			else if (PN == "A")   PColA   = P.PinId;
		}
	}

	uint32 MulRGB_A = 0, MulRGB_B = 0, MulRGB_Out = 0;
	if (FMaterialGraphNode* N = AddNodeOfType(EMaterialGraphNodeType::Multiply, -200.0f, 0.0f, Domain))
	{
		MulRGB_A   = N->Pins[0].PinId;
		MulRGB_B   = N->Pins[1].PinId;
		MulRGB_Out = N->Pins[2].PinId;
	}

	uint32 MulA_A = 0, MulA_B = 0, MulA_Out = 0;
	if (FMaterialGraphNode* N = AddNodeOfType(EMaterialGraphNodeType::Multiply, -200.0f, 180.0f, Domain))
	{
		MulA_A   = N->Pins[0].PinId;
		MulA_B   = N->Pins[1].PinId;
		MulA_Out = N->Pins[2].PinId;
	}

	// Output은 마지막 — 추가 push_back이 없으니 포인터를 잠시 유지해도 안전.
	FMaterialGraphNode* Out = AddNodeOfType(EMaterialGraphNodeType::Output, 80.0f, 80.0f, Domain);

	AddLink(TexOut,    SampleTexIn);
	AddLink(SampleRGB, MulRGB_A);
	AddLink(PColRGB,   MulRGB_B);
	AddLink(SampleA,   MulA_A);
	AddLink(PColA,     MulA_B);

	if (Out)
	{
		for (const FMaterialGraphPin& Pin : Out->Pins)
		{
			const FString PinName = Pin.DisplayName.ToString();
			if (PinName == "Color" || PinName == "BaseColor") AddLink(MulRGB_Out, Pin.PinId);
			else if (PinName == "Opacity")                     AddLink(MulA_Out,   Pin.PinId);
		}
	}
}

void FMaterialGraph::RebuildOutputPinsForDomain(EMaterialDomain Domain)
{
	FMaterialGraphNode* Output = FindFirstNodeOfType(EMaterialGraphNodeType::Output);
	if (!Output)
	{
		return;
	}

	TArray<uint32> OldPinIds;
	for (const FMaterialGraphPin& Pin : Output->Pins)
	{
		OldPinIds.push_back(Pin.PinId);
	}

	Links.erase(std::remove_if(Links.begin(), Links.end(),
		[&OldPinIds](const FMaterialGraphLink& L)
		{
			for (uint32 PinId : OldPinIds)
			{
				if (L.FromPinId == PinId || L.ToPinId == PinId) return true;
			}
			return false;
		}), Links.end());
	Output->Pins.clear();

	auto AddOutPin = [this, Output](const char* Name, EMaterialGraphPinType Type)
	{
		AddPin(*Output, EMaterialGraphPinKind::Input, Type, FName(Name));
	};

	switch (Domain)
	{
	case EMaterialDomain::ParticleSprite:
	case EMaterialDomain::ParticleMesh:
		AddOutPin("Color", EMaterialGraphPinType::Float3);
		AddOutPin("Emissive", EMaterialGraphPinType::Float3);
		AddOutPin("Opacity", EMaterialGraphPinType::Float);
		AddOutPin("UVOffset", EMaterialGraphPinType::Float2);
		break;
	case EMaterialDomain::Decal:
		AddOutPin("BaseColor", EMaterialGraphPinType::Float3);
		AddOutPin("Normal", EMaterialGraphPinType::Float3);
		AddOutPin("Roughness", EMaterialGraphPinType::Float);
		AddOutPin("Metallic", EMaterialGraphPinType::Float);
		AddOutPin("Opacity", EMaterialGraphPinType::Float);
		break;
	case EMaterialDomain::PostProcess:
		AddOutPin("Color", EMaterialGraphPinType::Float3);
		AddOutPin("Opacity", EMaterialGraphPinType::Float);
		break;
	case EMaterialDomain::Surface:
	default:
		AddOutPin("BaseColor", EMaterialGraphPinType::Float3);
		AddOutPin("Normal", EMaterialGraphPinType::Float3);
		AddOutPin("Roughness", EMaterialGraphPinType::Float);
		AddOutPin("Metallic", EMaterialGraphPinType::Float);
		AddOutPin("Emissive", EMaterialGraphPinType::Float3);
		AddOutPin("Opacity", EMaterialGraphPinType::Float);
		AddOutPin("OpacityMask", EMaterialGraphPinType::Float);
		break;
	}
}

const char* ToString(EMaterialDomain Domain)
{
	switch (Domain)
	{
	case EMaterialDomain::Surface: return "Surface";
	case EMaterialDomain::ParticleSprite: return "ParticleSprite";
	case EMaterialDomain::ParticleMesh: return "ParticleMesh";
	case EMaterialDomain::Decal: return "Decal";
	case EMaterialDomain::PostProcess: return "PostProcess";
	}
	return "Surface";
}

const char* ToString(EMaterialGraphPinType Type)
{
	switch (Type)
	{
	case EMaterialGraphPinType::Float: return "Float";
	case EMaterialGraphPinType::Float2: return "Float2";
	case EMaterialGraphPinType::Float3: return "Float3";
	case EMaterialGraphPinType::Float4: return "Float4";
	case EMaterialGraphPinType::Color: return "Color";
	case EMaterialGraphPinType::UV: return "UV";
	case EMaterialGraphPinType::Texture2D: return "Texture2D";
	case EMaterialGraphPinType::Sampler: return "Sampler";
	case EMaterialGraphPinType::Bool: return "Bool";
	}
	return "Float";
}

const char* ToString(EMaterialGraphNodeType Type)
{
	switch (Type)
	{
	case EMaterialGraphNodeType::Output: return "Output";
	case EMaterialGraphNodeType::TextureObject: return "TextureObject";
	case EMaterialGraphNodeType::TextureSample: return "TextureSample";
	case EMaterialGraphNodeType::ScalarParameter: return "ScalarParameter";
	case EMaterialGraphNodeType::VectorParameter: return "VectorParameter";
	case EMaterialGraphNodeType::ColorParameter: return "ColorParameter";
	case EMaterialGraphNodeType::ConstantFloat: return "ConstantFloat";
	case EMaterialGraphNodeType::ConstantFloat2: return "ConstantFloat2";
	case EMaterialGraphNodeType::ConstantFloat3: return "ConstantFloat3";
	case EMaterialGraphNodeType::ConstantFloat4: return "ConstantFloat4";
	case EMaterialGraphNodeType::Add: return "Add";
	case EMaterialGraphNodeType::Subtract: return "Subtract";
	case EMaterialGraphNodeType::Multiply: return "Multiply";
	case EMaterialGraphNodeType::Divide: return "Divide";
	case EMaterialGraphNodeType::OneMinus: return "OneMinus";
	case EMaterialGraphNodeType::Saturate: return "Saturate";
	case EMaterialGraphNodeType::Clamp: return "Clamp";
	case EMaterialGraphNodeType::Power: return "Power";
	case EMaterialGraphNodeType::Lerp: return "Lerp";
	case EMaterialGraphNodeType::TexCoord: return "TexCoord";
	case EMaterialGraphNodeType::Panner: return "Panner";
	case EMaterialGraphNodeType::Time: return "Time";
	case EMaterialGraphNodeType::VertexColor: return "VertexColor";
	case EMaterialGraphNodeType::ParticleColor: return "ParticleColor";
	case EMaterialGraphNodeType::Append: return "Append";
	case EMaterialGraphNodeType::ComponentMask: return "ComponentMask";
	case EMaterialGraphNodeType::ConstantBiasScale: return "ConstantBiasScale";
	case EMaterialGraphNodeType::Distance: return "Distance";
	case EMaterialGraphNodeType::Normalize: return "Normalize";
	case EMaterialGraphNodeType::Dot: return "Dot";
	case EMaterialGraphNodeType::Cross: return "Cross";
	}
	return "Output";
}

const char* ToString(EMaterialTextureSlot Slot)
{
	switch (Slot)
	{
	case EMaterialTextureSlot::Diffuse: return "Diffuse";
	case EMaterialTextureSlot::Normal: return "Normal";
	case EMaterialTextureSlot::Roughness: return "Roughness";
	case EMaterialTextureSlot::Metallic: return "Metallic";
	case EMaterialTextureSlot::Emissive: return "Emissive";
	case EMaterialTextureSlot::AO: return "AO";
	case EMaterialTextureSlot::Custom0: return "Custom0";
	case EMaterialTextureSlot::Custom1: return "Custom1";
	default: return "Diffuse";
	}
}

EMaterialDomain MaterialDomainFromString(const FString& Str, EMaterialDomain Default)
{
	if (Str == "Surface") return EMaterialDomain::Surface;
	if (Str == "ParticleSprite") return EMaterialDomain::ParticleSprite;
	if (Str == "ParticleMesh") return EMaterialDomain::ParticleMesh;
	if (Str == "Decal") return EMaterialDomain::Decal;
	if (Str == "PostProcess") return EMaterialDomain::PostProcess;
	return Default;
}

EMaterialGraphPinType MaterialPinTypeFromString(const FString& Str, EMaterialGraphPinType Default)
{
	if (Str == "Float") return EMaterialGraphPinType::Float;
	if (Str == "Float2") return EMaterialGraphPinType::Float2;
	if (Str == "Float3") return EMaterialGraphPinType::Float3;
	if (Str == "Float4") return EMaterialGraphPinType::Float4;
	if (Str == "Color") return EMaterialGraphPinType::Color;
	if (Str == "UV") return EMaterialGraphPinType::UV;
	if (Str == "Texture2D") return EMaterialGraphPinType::Texture2D;
	if (Str == "Sampler") return EMaterialGraphPinType::Sampler;
	if (Str == "Bool") return EMaterialGraphPinType::Bool;
	return Default;
}

EMaterialGraphNodeType MaterialNodeTypeFromString(const FString& Str, EMaterialGraphNodeType Default)
{
	for (int32 i = 0; i <= static_cast<int32>(EMaterialGraphNodeType::Cross); ++i)
	{
		const EMaterialGraphNodeType Type = static_cast<EMaterialGraphNodeType>(i);
		if (Str == ToString(Type)) return Type;
	}
	return Default;
}

EMaterialTextureSlot MaterialTextureSlotFromString(const FString& Str, EMaterialTextureSlot Default)
{
	if (Str == "Diffuse") return EMaterialTextureSlot::Diffuse;
	if (Str == "Normal") return EMaterialTextureSlot::Normal;
	if (Str == "Roughness") return EMaterialTextureSlot::Roughness;
	if (Str == "Metallic") return EMaterialTextureSlot::Metallic;
	if (Str == "Emissive") return EMaterialTextureSlot::Emissive;
	if (Str == "AO") return EMaterialTextureSlot::AO;
	if (Str == "Custom0") return EMaterialTextureSlot::Custom0;
	if (Str == "Custom1") return EMaterialTextureSlot::Custom1;
	return Default;
}

bool IsMaterialGraphPinTypeConvertible(EMaterialGraphPinType From, EMaterialGraphPinType To)
{
	if (From == To) return true;

	// vector ↔ vector 는 코드젠의 ConvertExpr이 broadcast/swizzle/zero-pad로 처리.
	// 링크는 허용하고 변환은 HLSL 생성 시점에 일관되게.
	auto IsVec = [](EMaterialGraphPinType T)
	{
		switch (T)
		{
		case EMaterialGraphPinType::Float:
		case EMaterialGraphPinType::Float2:
		case EMaterialGraphPinType::Float3:
		case EMaterialGraphPinType::Float4:
		case EMaterialGraphPinType::Color:
		case EMaterialGraphPinType::UV:
			return true;
		default:
			return false;
		}
	};
	if (IsVec(From) && IsVec(To)) return true;

	return false;
}

bool MaterialGraphAsset::LoadFromJson(const json::JSON& JsonData, FMaterialGraph& OutGraph)
{
	if (JsonData.IsNull() || JsonData.JSONType() != json::JSON::Class::Object)
	{
		return false;
	}

	OutGraph.Nodes.clear();
	OutGraph.Links.clear();
	OutGraph.NextId = JsonNumberToU32(JsonMember(JsonData, "NextId"), 1);

	const json::JSON& NodesJson = JsonMember(JsonData, "Nodes");
	if (NodesJson.JSONType() == json::JSON::Class::Array)
	{
		for (const json::JSON& NodeJson : NodesJson.ArrayRange())
		{
			FMaterialGraphNode Node;
			Node.NodeId = JsonNumberToU32(JsonMember(NodeJson, "NodeId"));
			Node.Type = MaterialNodeTypeFromString(JsonMember(NodeJson, "Type").ToString(), EMaterialGraphNodeType::Output);
			Node.DisplayName = FName(JsonMember(NodeJson, "DisplayName").ToString());
			Node.PosX = JsonNumberToFloat(JsonMember(NodeJson, "PosX"));
			Node.PosY = JsonNumberToFloat(JsonMember(NodeJson, "PosY"));
			Node.ParameterName = JsonMember(NodeJson, "ParameterName").ToString();
			Node.TexturePath = JsonMember(NodeJson, "TexturePath").ToString();
			Node.TextureSlot = MaterialTextureSlotFromString(JsonMember(NodeJson, "TextureSlot").ToString(), EMaterialTextureSlot::Diffuse);
			Node.Value = VectorFromJson(JsonMember(NodeJson, "Value"), Node.Value);
			Node.Mask = JsonMember(NodeJson, "Mask").ToString();
			if (Node.Mask.empty()) Node.Mask = "RGBA";

			const json::JSON& PinsJson = JsonMember(NodeJson, "Pins");
			if (PinsJson.JSONType() == json::JSON::Class::Array)
			{
				for (const json::JSON& PinJson : PinsJson.ArrayRange())
				{
					Node.Pins.push_back(LoadPin(PinJson));
				}
			}
			OutGraph.Nodes.push_back(std::move(Node));
		}
	}

	const json::JSON& LinksJson = JsonMember(JsonData, "Links");
	if (LinksJson.JSONType() == json::JSON::Class::Array)
	{
		for (const json::JSON& LinkJson : LinksJson.ArrayRange())
		{
			FMaterialGraphLink Link;
			Link.LinkId = JsonNumberToU32(JsonMember(LinkJson, "LinkId"));
			Link.FromPinId = JsonNumberToU32(JsonMember(LinkJson, "FromPinId"));
			Link.ToPinId = JsonNumberToU32(JsonMember(LinkJson, "ToPinId"));
			OutGraph.Links.push_back(Link);
		}
	}

	return true;
}

void MaterialGraphAsset::SaveToJson(const FMaterialGraph& Graph, json::JSON& OutJson)
{
	OutJson = json::JSON::Make(json::JSON::Class::Object);
	OutJson["NextId"] = static_cast<int>(Graph.NextId);
	OutJson["Nodes"] = json::Array();
	OutJson["Links"] = json::Array();

	uint32 NodeIndex = 0;
	for (const FMaterialGraphNode& Node : Graph.Nodes)
	{
		json::JSON NodeJson = json::JSON::Make(json::JSON::Class::Object);
		NodeJson["NodeId"] = static_cast<int>(Node.NodeId);
		NodeJson["Type"] = ToString(Node.Type);
		NodeJson["DisplayName"] = Node.DisplayName.ToString();
		NodeJson["PosX"] = Node.PosX;
		NodeJson["PosY"] = Node.PosY;
		NodeJson["ParameterName"] = Node.ParameterName;
		NodeJson["TexturePath"] = Node.TexturePath;
		NodeJson["TextureSlot"] = ToString(Node.TextureSlot);
		NodeJson["Value"] = VectorToJson(Node.Value);
		NodeJson["Mask"] = Node.Mask;
		NodeJson["Pins"] = json::Array();

		uint32 PinIndex = 0;
		for (const FMaterialGraphPin& Pin : Node.Pins)
		{
			json::JSON PinJson = json::JSON::Make(json::JSON::Class::Object);
			SavePin(Pin, PinJson);
			NodeJson["Pins"][PinIndex++] = std::move(PinJson);
		}

		OutJson["Nodes"][NodeIndex++] = std::move(NodeJson);
	}

	uint32 LinkIndex = 0;
	for (const FMaterialGraphLink& Link : Graph.Links)
	{
		json::JSON LinkJson = json::JSON::Make(json::JSON::Class::Object);
		LinkJson["LinkId"] = static_cast<int>(Link.LinkId);
		LinkJson["FromPinId"] = static_cast<int>(Link.FromPinId);
		LinkJson["ToPinId"] = static_cast<int>(Link.ToPinId);
		OutJson["Links"][LinkIndex++] = std::move(LinkJson);
	}
}

json::JSON MaterialGraphAsset::MakeDefaultMaterialJson(const FString& ProjectRelativePath, const FString& MaterialGuid)
{
	FMaterialGraph Graph;
	Graph.InitializeDefault(EMaterialDomain::ParticleSprite);

	json::JSON GraphJson;
	SaveToJson(Graph, GraphJson);

	json::JSON Root = json::JSON::Make(json::JSON::Class::Object);
	Root["Version"] = 2;
	Root["MaterialGuid"] = MaterialGuid;
	Root["PathFileName"] = ProjectRelativePath;
	Root["Domain"] = "ParticleSprite";
	Root["RenderPass"] = "AlphaBlend";
	Root["BlendState"] = "AlphaBlend";
	Root["DepthStencilState"] = "DepthReadOnly";
	Root["RasterizerState"] = "SolidNoCull";
	Root["GeneratedShaderPath"] = "";
	Root["Graph"] = std::move(GraphJson);
	Root["Compiled"] = json::JSON::Make(json::JSON::Class::Object);
	Root["Compiled"]["Parameters"] = json::JSON::Make(json::JSON::Class::Object);
	Root["Compiled"]["Textures"] = json::JSON::Make(json::JSON::Class::Object);
	return Root;
}

FString MaterialGraphAsset::ComputeGraphHashString(const json::JSON& GraphJson)
{
	// FNV-1a 64-bit. std::hash는 프로세스 실행마다 결과가 바뀔 수 있어 캐시 무효화가 매번 발생.
	const FString Dump = GraphJson.dump();
	constexpr uint64 FnvOffset = 0xcbf29ce484222325ULL;
	constexpr uint64 FnvPrime = 0x100000001b3ULL;
	uint64 Hash = FnvOffset;
	for (unsigned char Ch : Dump)
	{
		Hash ^= static_cast<uint64>(Ch);
		Hash *= FnvPrime;
	}
	char Buffer[32];
	std::snprintf(Buffer, sizeof(Buffer), "%016llX", static_cast<unsigned long long>(Hash));
	return Buffer;
}
