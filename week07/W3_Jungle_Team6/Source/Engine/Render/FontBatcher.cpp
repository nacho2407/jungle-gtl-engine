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
	const int32 ColCount = 16;
	const float CellW = 1.0f / ColCount;   // 텍스쳐 정규화 너비
	const float CellH = 1.0f / ColCount;    // 텍스쳐 정규화 높이 (96개 / 16열 = 6행)

	for (int i = 0; i < 96; ++i)           // ASCII 32(space) ~ 127
	{
		char ch = static_cast<char>(32 + i);
		int col = i % ColCount;
		int row = i / ColCount;

		charInfoMap[ch] = { col * CellW, row * CellH, CellW, CellH };
	}

	UE_LOG("Texture Load!");
}

void FFontBatcher::Release()
{
}

void FFontBatcher::AddText(const FString& Text,
	const FVector& WorldPos,
	const FVector& CamRight,
	const FVector& CamUp,
	const FColor& Color,
	float Scale)
{
}

void FFontBatcher::Clear()
{
	Vertices.clear();
	Indices.clear();
}

void FFontBatcher::Flush(ID3D11DeviceContext* Context, const FMatrix& ViewProj)
{
}

uint32 FFontBatcher::GetQuadCount() const
{
	return static_cast<uint32>(Vertices.size() / 4);
}

void FFontBatcher::GetCharUV(char Ch, FVector2& OutUVMin, FVector2& OutUVMax) const
{
	OutUVMin = FVector2(0.0f, 0.0f);
	OutUVMax = FVector2(1.0f, 1.0f);
}
