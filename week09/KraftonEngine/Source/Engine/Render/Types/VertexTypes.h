#pragma once

#include "Math/Vector.h"
#include "Render/Types/RenderTypes.h"
#include <cstddef>

struct FVertex
{
	FVector Position;
	FVector4 Color;
	int SubID;
};

struct FOverlayVertex
{
	float X, Y;
};

// Position + TexCoord 범용 버텍스 (FontBatcher, SubUVBatcher 등 텍스처 기반 배처 공용)
struct FTextureVertex
{
	FVector  Position;
	FVector2 TexCoord;
};

// Position + Normal + Color + UV (StaticMesh GPU용 정점 형식)
struct FVertexPNCT
{
	FVector Position;
	FVector Normal;
	FVector4 Color;
	FVector2 UV;
};

template<typename VertexType>
struct TMeshData
{
	TArray<VertexType> Vertices;
	TArray<uint32> Indices;
};

using FMeshData = TMeshData<FVertex>;

// InputLayout — 각 정점 구조체에 대응하는 D3D11 입력 레이아웃
inline D3D11_INPUT_ELEMENT_DESC FVertexInputLayout[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, static_cast<uint32>(offsetof(FVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FVertex, Color)),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

inline D3D11_INPUT_ELEMENT_DESC FVertexPNCTInputLayout[] =
{
	{ "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, static_cast<uint32>(offsetof(FVertexPNCT, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, static_cast<uint32>(offsetof(FVertexPNCT, Normal)),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",     0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FVertexPNCT, Color)),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXTCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, static_cast<uint32>(offsetof(FVertexPNCT, UV)),       D3D11_INPUT_PER_VERTEX_DATA, 0 },
};