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

	// Sampler — Point 필터 (폰트는 선명하게)
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	hr = Device->CreateSamplerState(&sampDesc, &SamplerState);
	if (FAILED(hr)) return;

	// 상수버퍼
	FontCB.Create(Device, sizeof(FFontTransformConstants));

	// 셰이더 + Input Layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	FontShader.Create(Device, L"Shaders/ShaderFont.hlsl",
		"FontVS", "FontPS", layout, ARRAYSIZE(layout));
}

// Atlas 텍스처 슬라이싱
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

	if (FontResource) { FontResource->Release(); FontResource = nullptr; }
	if (FontAtlasSRV) { FontAtlasSRV->Release(); FontAtlasSRV = nullptr; }
	if (SamplerState) { SamplerState->Release(); SamplerState = nullptr; }
	FontShader.Release();
	FontCB.Release();
}

FFontMesh FFontBatcher::BuildMesh(ID3D11Device* Device, const FString& Text)
{
	TArray<FFontVertex> Vertices;
	TArray<uint32>      Indices;

	const float CharW     = 0.5f;
	const float CharH     = 0.5f;
	float       CharCursorX = 0.f;
	uint32      Base      = 0;

	for (const auto& Ch : Text)
	{
		FVector2 UVMin, UVMax;
		GetCharUV(Ch, UVMin, UVMax);

		// 로컬 오프셋 저장 (카메라 벡터는 상수버퍼로 전달)
		Vertices.push_back({ { CharCursorX,          CharH * 0.5f,  0 }, { UVMin.X, UVMin.Y } });
		Vertices.push_back({ { CharCursorX + CharW,  CharH * 0.5f,  0 }, { UVMax.X, UVMin.Y } });
		Vertices.push_back({ { CharCursorX,         -CharH * 0.5f,  0 }, { UVMin.X, UVMax.Y } });
		Vertices.push_back({ { CharCursorX + CharW, -CharH * 0.5f,  0 }, { UVMax.X, UVMax.Y } });

		Indices.push_back(Base + 0); Indices.push_back(Base + 1); Indices.push_back(Base + 2);
		Indices.push_back(Base + 1); Indices.push_back(Base + 3); Indices.push_back(Base + 2);

		CharCursorX += CharW;
		Base += 4;
	}

	FFontMesh Mesh;

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.Usage     = D3D11_USAGE_IMMUTABLE;
	vbDesc.ByteWidth = sizeof(FFontVertex) * static_cast<uint32>(Vertices.size());
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vbData = { Vertices.data() };
	Device->CreateBuffer(&vbDesc, &vbData, &Mesh.VertexBuffer);

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.Usage     = D3D11_USAGE_IMMUTABLE;
	ibDesc.ByteWidth = sizeof(uint32) * static_cast<uint32>(Indices.size());
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA ibData = { Indices.data() };
	Device->CreateBuffer(&ibDesc, &ibData, &Mesh.IndexBuffer);

	Mesh.IndexCount = static_cast<uint32>(Indices.size());
	return Mesh;
}

void FFontBatcher::DrawText(ID3D11DeviceContext* Context,
	const FFontMesh& Mesh,
	const FVector&   WorldPos,
	const FVector&   CamRight,
	const FVector&   CamUp,
	const FMatrix&   ViewProjection)
{
	if (!Mesh.IsValid()) return;

	// 상수 버퍼 업데이트 및 연결
	FFontTransformConstants cb;
	cb.ViewProj = ViewProjection;
	cb.WorldPos = WorldPos;
	cb.CamRight = CamRight;
	cb.CamUp    = CamUp;
	FontCB.Update(Context, &cb, sizeof(cb));

	ID3D11Buffer* cbBuf = FontCB.GetBuffer();
	Context->VSSetConstantBuffers(0, 1, &cbBuf);

	FontShader.Bind(Context);

	uint32 stride = sizeof(FFontVertex), offset = 0;
	Context->IASetVertexBuffers(0, 1, &Mesh.VertexBuffer, &stride, &offset);
	Context->IASetIndexBuffer(Mesh.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	Context->PSSetShaderResources(0, 1, &FontAtlasSRV);
	Context->PSSetSamplers(0, 1, &SamplerState);

	Context->DrawIndexed(Mesh.IndexCount, 0, 0);
}

void FFontBatcher::GetCharUV(char Key, FVector2& OutUVMin, FVector2& OutUVMax) const
{
	auto it = CharInfoMap.find(Key);

	if (it == CharInfoMap.end()) {
		OutUVMin = FVector2(0, 0); OutUVMax = FVector2(1, 1);
		return;
	}

	const FCharacterInfo& Info = it->second;
	OutUVMin = FVector2(Info.u, Info.v);
	OutUVMax = FVector2(Info.u + Info.width, Info.v + Info.height);
}
