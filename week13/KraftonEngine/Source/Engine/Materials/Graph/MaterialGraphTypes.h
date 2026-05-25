#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Object/FName.h"
#include "Render/Types/MaterialTextureSlot.h"
#include "Render/Types/RenderStateTypes.h"
#include "Render/Types/RenderTypes.h"

enum class EMaterialDomain : uint8
{
	Surface,
	ParticleSprite,
	ParticleMesh,
	Decal,
	PostProcess
};

enum class EMaterialGraphPinKind : uint8
{
	Input,
	Output
};

enum class EMaterialGraphPinType : uint8
{
	Float,
	Float2,
	Float3,
	Float4,
	Color,
	UV,
	Texture2D,
	Sampler,
	Bool
};

enum class EMaterialGraphNodeType : uint8
{
	Output,

	TextureObject,
	TextureSample,

	ScalarParameter,
	VectorParameter,
	ColorParameter,

	ConstantFloat,
	ConstantFloat2,
	ConstantFloat3,
	ConstantFloat4,

	Add,
	Subtract,
	Multiply,
	Divide,
	OneMinus,
	Saturate,
	Clamp,
	Power,

	Lerp,

	TexCoord,
	Panner,
	Time,

	VertexColor,
	ParticleColor,

	Append,
	ComponentMask,

	// 추가 수학 노드
	ConstantBiasScale,   // V * Scale + Bias
	Distance,            // length(A - B) → Float
	Normalize,           // normalize(V)
	Dot,                 // dot(A, B) → Float
	Cross,               // cross(A, B) → Float3
};

struct FMaterialGraphPin
{
	uint32                 PinId = 0;
	uint32                 OwningNodeId = 0;
	EMaterialGraphPinKind Kind = EMaterialGraphPinKind::Input;
	EMaterialGraphPinType Type = EMaterialGraphPinType::Float;
	FName                  DisplayName;
};

struct FMaterialGraphLink
{
	uint32 LinkId = 0;
	uint32 FromPinId = 0;
	uint32 ToPinId = 0;
};

struct FMaterialGraphNode
{
	uint32                    NodeId = 0;
	EMaterialGraphNodeType    Type = EMaterialGraphNodeType::Output;
	FName                     DisplayName;
	float                     PosX = 0.0f;
	float                     PosY = 0.0f;
	TArray<FMaterialGraphPin> Pins;

	FString                   ParameterName;
	FString                   TexturePath;
	EMaterialTextureSlot      TextureSlot = EMaterialTextureSlot::Diffuse;
	FVector4                  Value = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	FString                   Mask = "RGBA";
};

struct FMaterialCompiledParameter
{
	EMaterialGraphPinType Type = EMaterialGraphPinType::Float4;
	FVector4              Value = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
};

struct FMaterialCompiledTexture
{
	FString              Path;
	EMaterialTextureSlot Slot = EMaterialTextureSlot::Diffuse;
};

struct FMaterialGraph
{
	TArray<FMaterialGraphNode> Nodes;
	TArray<FMaterialGraphLink> Links;
	uint32                     NextId = 1;

	uint32 AllocateId() { return NextId++; }

	FMaterialGraphNode* AddNode(EMaterialGraphNodeType Type, const FName& DisplayName, float X, float Y);
	FMaterialGraphPin*  AddPin(FMaterialGraphNode& Node, EMaterialGraphPinKind Kind, EMaterialGraphPinType PinType, const FName& DisplayName);
	FMaterialGraphLink* AddLink(uint32 FromPinId, uint32 ToPinId);
	FMaterialGraphNode* AddNodeOfType(EMaterialGraphNodeType Type, float X, float Y, EMaterialDomain Domain);

	bool RemoveNode(uint32 NodeId);
	bool RemoveLink(uint32 LinkId);
	bool CanLinkPins(uint32 PinAId, uint32 PinBId, uint32* OutFromPinId = nullptr, uint32* OutToPinId = nullptr) const;
	bool HasOutputNode() const;

	FMaterialGraphNode*       FindNode(uint32 NodeId);
	const FMaterialGraphNode* FindNode(uint32 NodeId) const;
	FMaterialGraphPin*        FindPin(uint32 PinId);
	const FMaterialGraphPin*  FindPin(uint32 PinId) const;
	FMaterialGraphNode*       FindFirstNodeOfType(EMaterialGraphNodeType Type);
	const FMaterialGraphNode* FindFirstNodeOfType(EMaterialGraphNodeType Type) const;

	void InitializeDefault(EMaterialDomain Domain);
	void RebuildOutputPinsForDomain(EMaterialDomain Domain);

	// 텍스처 기반 파티클 프리셋 — 한 번에 TextureObject/Sample/Multiply/Mask/Output 세팅.
	// 호출 시 기존 노드/링크 모두 클리어.
	void ApplyTexturedParticlePreset(EMaterialDomain Domain);
};

const char* ToString(EMaterialDomain Domain);
const char* ToString(EMaterialGraphPinType Type);
const char* ToString(EMaterialGraphNodeType Type);
const char* ToString(EMaterialTextureSlot Slot);

EMaterialDomain        MaterialDomainFromString(const FString& Str, EMaterialDomain Default = EMaterialDomain::Surface);
EMaterialGraphPinType  MaterialPinTypeFromString(const FString& Str, EMaterialGraphPinType Default = EMaterialGraphPinType::Float);
EMaterialGraphNodeType MaterialNodeTypeFromString(const FString& Str, EMaterialGraphNodeType Default = EMaterialGraphNodeType::Output);
EMaterialTextureSlot   MaterialTextureSlotFromString(const FString& Str, EMaterialTextureSlot Default = EMaterialTextureSlot::Diffuse);

bool IsMaterialGraphPinTypeConvertible(EMaterialGraphPinType From, EMaterialGraphPinType To);
