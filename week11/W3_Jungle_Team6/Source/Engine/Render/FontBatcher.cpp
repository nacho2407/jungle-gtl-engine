#include "FontBatcher.h"

// DirectXTK 라이브러리
#pragma comment(lib, "DirectXTK.lib")
#include "DDSTextureLoader.h"
#include "Editor/Core/EditorConsole.h"
#include "Core/CoreTypes.h"


void FFontBatcher::Create(ID3D11Device* Device)
{
	HRESULT hr = DirectX::CreateDDSTextureFromFileEx(
		Device,
		L"./Resources/Textures/FontAtlas.dds",
		0,
		D3D11_USAGE_IMMUTABLE,
		D3D11_BIND_SHADER_RESOURCE,
		0,
		0,
		DirectX::DDS_LOADER_DEFAULT,
		&FontResource,
		&FontAtlasSRV
	);

	if (FAILED(hr))
	{
		return;
	}

	BuildCharInfoMap();

	FVector2 OutX, OutY;
	GetCharUV('c', OutX, OutY);
}

void FFontBatcher::BuildCharInfoMap()
{
	const int32 Count = 16;
	const float CellW = 1.0f / Count;
	const float CellH = 1.0f / Count;

	// ASCII 32(space) ~ 127
	for (int32 i = 0; i < 16; ++i)
	{
		for (int32 j = 0; j < 16; ++j)
		{
			int32 Idx = i * 16 + j;
			if (Idx < 32) continue;
			if (Idx > 126) return;

			CharInfoMap[static_cast<char>(i * 16 + j)] = { j * CellW, i * CellH, CellW, CellH };
		}
	}
}

void FFontBatcher::Release()
{
	CharInfoMap.clear();
	Clear();

	// DX11 Release
	VertexBuffer->Release();
	IndexBuffer->Release();
}


void FFontBatcher::AddText(const FString& Text, const FVector& WorldPos)
{
	const float CharW = 0.5f;
	const float CharH = 0.8f;
	float PenX = 0.f;

	for (const auto& Ch : Text)
	{
		FVector2 UVMin, UVMax;
		GetCharUV(Ch, UVMin, UVMax); 

		uint32 Base = static_cast<uint32>(Vertices.size());

		// 4개의 정점 추가
		Vertices.push_back({ {PenX,         +CharH}, {UVMin.X, UVMin.Y} });
		Vertices.push_back({ {PenX + CharW, +CharH}, {UVMax.X, UVMin.Y} });
		Vertices.push_back({ {PenX,         -CharH}, {UVMin.X, UVMax.Y} });
		Vertices.push_back({ {PenX + CharW, -CharH}, {UVMax.X, UVMax.Y} });

		// 6 개의 인덱스 정보 추가
		// 
		// Triangle 1: v0 → v2 → v1  (CW)
		Indices.push_back(Base + 0);
		Indices.push_back(Base + 2);
		Indices.push_back(Base + 1);

		// Triangle 2: v1 → v2 → v3  (CW)
		Indices.push_back(Base + 1);
		Indices.push_back(Base + 2);
		Indices.push_back(Base + 3);

		PenX += CharW;
	}

	// Vertex Buffer, Index Buffer 위치?
}

void FFontBatcher::Clear()
{
	Vertices.clear();
	Indices.clear();
}

void FFontBatcher::Flush(ID3D11DeviceContext* Context, const FMatrix& View, const FMatrix& Proj)
{

	
}

uint32 FFontBatcher::GetQuadCount() const
{
	return static_cast<uint32>(Vertices.size() / 4);
}

void FFontBatcher::GetCharUV(char Ch, FVector2& OutUVMin, FVector2& OutUVMax) const
{
	auto it = CharInfoMap.find(Ch);

	if (it == CharInfoMap.end()) {
		OutUVMin = FVector2(0, 0); OutUVMax = FVector2(1, 1);
		return;
	}

	const FCharacterInfo& ci = it->second;
	OutUVMin = FVector2(ci.u, ci.v);
	OutUVMax = FVector2(ci.u + ci.width, ci.v - ci.height);
}
