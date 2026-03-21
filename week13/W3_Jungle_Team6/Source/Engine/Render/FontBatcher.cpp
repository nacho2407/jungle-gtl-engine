#include "FontBatcher.h"

// DirectXTK 라이브러리
#pragma comment(lib, "DirectXTK.lib")
#include "ThirdParty/DirectXTK/DDSTextureLoader.h"
#include "Core/CoreTypes.h"

void FFontBatcher::Create(ID3D11Device* InDevice)
{
	Device = InDevice;

	// 폰트 아틀라스 텍스처 로드
	HRESULT hr = DirectX::CreateDDSTextureFromFileEx(
		Device,
		L"./Resources/Textures/FontAtlas.dds",
		0,
		D3D11_USAGE_IMMUTABLE,
		D3D11_BIND_SHADER_RESOURCE,
		0, 0,
		DirectX::DDS_LOADER_DEFAULT,
		&FontResource,
		&FontAtlasSRV
	);
	if (FAILED(hr)) return;

	BuildCharInfoMap();

	// Dynamic VB/IB 초기 할당
	MaxVertexCount = 1024;
	MaxIndexCount  = 1536;
	CreateBuffers();

	// Sampler — Point 필터 (폰트는 선명하게)
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter   = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	hr = Device->CreateSamplerState(&sampDesc, &SamplerState);
	if (FAILED(hr)) return;

	// 셰이더 + Input Layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	FontShader.Create(Device, L"Shaders/ShaderFont.hlsl",
		"VS", "PS", layout, ARRAYSIZE(layout));
}

void FFontBatcher::CreateBuffers()
{
	// 버퍼가 존재한다면 해제(재할당 목적)
	if (VertexBuffer) { VertexBuffer->Release(); VertexBuffer = nullptr; }
	if (IndexBuffer)  { IndexBuffer->Release();  IndexBuffer  = nullptr; }

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.Usage          = D3D11_USAGE_DYNAMIC;
	vbDesc.ByteWidth      = sizeof(FFontVertex) * MaxVertexCount;
	vbDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Device->CreateBuffer(&vbDesc, nullptr, &VertexBuffer);

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.Usage          = D3D11_USAGE_DYNAMIC;
	ibDesc.ByteWidth      = sizeof(uint32) * MaxIndexCount;
	ibDesc.BindFlags      = D3D11_BIND_INDEX_BUFFER;
	ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Device->CreateBuffer(&ibDesc, nullptr, &IndexBuffer);
}

// Atlas 텍스처 슬라이싱
void FFontBatcher::BuildCharInfoMap()
{
	const int32 Count = 16;
	const float CellW = 1.0f / Count;
	const float CellH = 1.0f / Count;

	// ASCII 32(space) ~ 126
	for (int32 i = 0; i < 16; ++i)
	{
		for (int32 j = 0; j < 16; ++j)
		{
			int32 Idx = i * 16 + j;
			if (Idx < 32)  continue;
			if (Idx > 126) return;

			CharInfoMap[static_cast<char>(Idx)] = { j * CellW, i * CellH, CellW, CellH };
		}
	}
}

void FFontBatcher::Release()
{
	// Device는 소유권이 나에게 없음.
	CharInfoMap.clear();
	Clear();

	if (VertexBuffer) { VertexBuffer->Release(); VertexBuffer = nullptr; }
	if (IndexBuffer)  { IndexBuffer->Release();  IndexBuffer  = nullptr; }
	if (FontResource) { FontResource->Release(); FontResource = nullptr; }
	if (FontAtlasSRV) { FontAtlasSRV->Release(); FontAtlasSRV = nullptr; }
	if (SamplerState) { SamplerState->Release(); SamplerState = nullptr; }
	
	FontShader.Release();

}

void FFontBatcher::AddText(const FString& Text,
	const FVector& WorldPos,
	const FVector& CamRight,
	const FVector& CamUp,
	float Scale/* = 1.0f*/)
{
	const float CharW = 0.5f * Scale;
	const float CharH = 0.5f * Scale;
	float CharCursorX = 0.f;
	uint32 Base = static_cast<uint32>(Vertices.size());

	for (const auto& Ch : Text)
	{
		FVector2 UVMin, UVMax;
		GetCharUV(Ch, UVMin, UVMax);

		// CPU Billboard — CamRight/Up으로 월드 좌표 직접 계산
		/*
		* 계산식
		* - WorldPos: 나의 월드 좌표계 상의 위치
		* - CamRight: 내가 가로로 뻗어나갈 방향
		* - CamUp: 내가 세로로 뻗어나갈 방향
		*/
		FVector Center = WorldPos + CamRight * (CharCursorX/* + CharW * 0.5f*/);

		FVector v0 = Center + CamRight * (-CharW * 0.5f) + CamUp * ( CharH * 0.5f); // 좌상
		FVector v1 = Center + CamRight * ( CharW * 0.5f) + CamUp * ( CharH * 0.5f); // 우상
		FVector v2 = Center + CamRight * (-CharW * 0.5f) + CamUp * (-CharH * 0.5f); // 좌하
		FVector v3 = Center + CamRight * ( CharW * 0.5f) + CamUp * (-CharH * 0.5f); // 우하

		Vertices.push_back({ v0, { UVMin.X, UVMin.Y } });
		Vertices.push_back({ v1, { UVMax.X, UVMin.Y } });
		Vertices.push_back({ v2, { UVMin.X, UVMax.Y } });
		Vertices.push_back({ v3, { UVMax.X, UVMax.Y } });

		Indices.push_back(Base + 0); Indices.push_back(Base + 1); Indices.push_back(Base + 2);
		Indices.push_back(Base + 1); Indices.push_back(Base + 3); Indices.push_back(Base + 2);

		CharCursorX += CharW;
		Base += 4;
	}
}

void FFontBatcher::Clear()
{
	Vertices.clear();
	Indices.clear();
}

void FFontBatcher::Flush(ID3D11DeviceContext* Context)
{
	if (Vertices.empty() || !VertexBuffer || !IndexBuffer) return;

	// 버퍼 크기 초과 시 재할당
	if (Vertices.size() > MaxVertexCount || Indices.size() > MaxIndexCount)
	{
		MaxVertexCount = static_cast<uint32>(Vertices.size()) * 2;
		MaxIndexCount  = static_cast<uint32>(Indices.size())  * 2;
		CreateBuffers();
	}

	// VB 업로드
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(Context->Map(VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
	memcpy(mapped.pData, Vertices.data(), sizeof(FFontVertex) * Vertices.size());
	Context->Unmap(VertexBuffer, 0);

	// IB 업로드
	if (FAILED(Context->Map(IndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
	memcpy(mapped.pData, Indices.data(), sizeof(uint32) * Indices.size());
	Context->Unmap(IndexBuffer, 0);

	// Binding Shader
	FontShader.Bind(Context);

	uint32 stride = sizeof(FFontVertex), offset = 0;
	Context->IASetVertexBuffers(0, 1, &VertexBuffer, &stride, &offset);
	Context->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	Context->PSSetShaderResources(0, 1, &FontAtlasSRV);
	Context->PSSetSamplers(0, 1, &SamplerState);

	Context->DrawIndexed(static_cast<uint32>(Indices.size()), 0, 0);
}

void FFontBatcher::GetCharUV(char Key, FVector2& OutUVMin, FVector2& OutUVMax) const
{
	auto it = CharInfoMap.find(Key);
	if (it == CharInfoMap.end())
	{
		OutUVMin = FVector2(0, 0);
		OutUVMax = FVector2(1, 1);
		return;
	}

	const FCharacterInfo& Info = it->second;
	OutUVMin = FVector2(Info.u, Info.v);
	OutUVMax = FVector2(Info.u + Info.width, Info.v + Info.height);
}
