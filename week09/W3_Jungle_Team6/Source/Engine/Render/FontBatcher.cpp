#include "FontBatcher.h"

#include "Core/CoreTypes.h"

void FFontBatcher::Create(ID3D11Device* InDevice)
{
	Device = InDevice;

	// Dynamic VB/IB 초기 할당 (텍스처는 ResourceManager가 소유 — 여기서 로드하지 않음)
	MaxVertexCount = 1024;
	MaxIndexCount = 1536;
	CreateBuffers();

	// Sampler — Point 필터 (폰트는 선명하게)
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	Device->CreateSamplerState(&sampDesc, &SamplerState);

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
	if (VertexBuffer) { VertexBuffer->Release(); VertexBuffer = nullptr; }
	if (IndexBuffer) { IndexBuffer->Release();  IndexBuffer = nullptr; }

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.Usage = D3D11_USAGE_DYNAMIC;
	vbDesc.ByteWidth = sizeof(FTextureVertex) * MaxVertexCount;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Device->CreateBuffer(&vbDesc, nullptr, &VertexBuffer);

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.Usage = D3D11_USAGE_DYNAMIC;
	ibDesc.ByteWidth = sizeof(uint32) * MaxIndexCount;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Device->CreateBuffer(&ibDesc, nullptr, &IndexBuffer);
}

void FFontBatcher::BuildCharInfoMap(uint32 Columns, uint32 Rows)
{
	CharInfoMap.clear();
	CachedColumns = Columns;
	CachedRows = Rows;

	const float CellW = 1.0f / static_cast<float>(Columns);
	const float CellH = 1.0f / static_cast<float>(Rows);

	auto AddChar = [&](uint32 Codepoint, uint32 Slot)
	{
		const uint32 Col = Slot % Columns;
		const uint32 Row = Slot / Columns;
		if (Row >= Rows) return;
		CharInfoMap[Codepoint] = { Col * CellW, Row * CellH, CellW, CellH };
	};

	// ASCII 33(!) ~ 126(~) : 슬롯 = 코드포인트 (원본 아틀라스 배치 그대로)
	for (uint32 CP = 32; CP <= 126; ++CP)
		AddChar(CP, CP - 32);

	// 한글 완성형 가(U+AC00) ~ 힣(U+D7A3) : ASCII 다음 슬롯부터
	uint32 Slot = 127;
	for (uint32 CP = 0xAC00; CP <= 0xD7A3; ++CP, ++Slot)
		AddChar(CP, Slot - 32);
}

void FFontBatcher::Release()
{
	CharInfoMap.clear();
	Clear();

	if (VertexBuffer) { VertexBuffer->Release(); VertexBuffer = nullptr; }
	if (IndexBuffer) { IndexBuffer->Release();  IndexBuffer = nullptr; }
	if (SamplerState) { SamplerState->Release(); SamplerState = nullptr; }

	FontShader.Release();
}

void FFontBatcher::AddText(const FString& Text,
	const FVector& WorldPos,
	const FVector& CamRight,
	const FVector& CamUp,
	float Scale)
{
	if (Text.empty()) return;

	const float CharW = 0.5f * Scale;
	const float CharH = 0.5f * Scale;
	float CharCursorX = 0.f;
	const uint32 Base = static_cast<uint32>(Vertices.size());
	const uint32 IdxBase = static_cast<uint32>(Indices.size());
	const size_t CharCount = Text.size();

	// resize + 포인터 직접 쓰기로 push_back 오버헤드 제거
	Vertices.resize(Base + CharCount * 4);
	Indices.resize(IdxBase + CharCount * 6);
	FTextureVertex* pV = Vertices.data() + Base;
	uint32* pI = Indices.data() + IdxBase;

	// 빌보드 반벡터를 루프 밖에서 미리 계산
	const FVector HalfRight = CamRight * (CharW * 0.5f);
	const FVector HalfUp    = CamUp    * (CharH * 0.5f);

	const uint8* Ptr = reinterpret_cast<const uint8*>(Text.c_str());
	const uint8* const End = Ptr + Text.size();
	uint32 CharIdx = 0;

	for (size_t i = 0; i < CharCount && Ptr < End; ++i)
	{
		uint32 CP = 0;
		if      (Ptr[0] < 0x80)                             { CP = Ptr[0];                                                                       Ptr += 1; }
		else if ((Ptr[0] & 0xE0) == 0xC0 && Ptr + 1 < End)  { CP = ((Ptr[0] & 0x1F) << 6)  |  (Ptr[1] & 0x3F);                                   Ptr += 2; }
		else if ((Ptr[0] & 0xF0) == 0xE0 && Ptr + 2 < End)  { CP = ((Ptr[0] & 0x0F) << 12) | ((Ptr[1] & 0x3F) << 6)  |  (Ptr[2] & 0x3F);         Ptr += 3; }
		else if ((Ptr[0] & 0xF8) == 0xF0 && Ptr + 3 < End)  { CP = ((Ptr[0] & 0x07) << 18) | ((Ptr[1] & 0x3F) << 12) | ((Ptr[2] & 0x3F) << 6) | (Ptr[3] & 0x3F); Ptr += 4; }
		else												{ ++Ptr; continue; }

		FVector2 UVMin, UVMax;
		GetCharUV(CP, UVMin, UVMax);

		const FVector Center = WorldPos + CamRight * CharCursorX;

		pV[0] = { Center - HalfRight + HalfUp, { UVMin.X, UVMin.Y } };
		pV[1] = { Center + HalfRight + HalfUp, { UVMax.X, UVMin.Y } };
		pV[2] = { Center - HalfRight - HalfUp, { UVMin.X, UVMax.Y } };
		pV[3] = { Center + HalfRight - HalfUp, { UVMax.X, UVMax.Y } };

		const uint32 Vi = Base + CharIdx * 4;
		pI[0] = Vi;     pI[1] = Vi + 1; pI[2] = Vi + 2;
		pI[3] = Vi + 1; pI[4] = Vi + 3; pI[5] = Vi + 2;

		pV += 4;
		pI += 6;
		++CharIdx;
		CharCursorX += CharW;
	}

	// 실제 출력된 문자 수에 맞게 배열 크기 조정 (멀티바이트 문자 처리 후 잉여 슬롯 제거)
	Vertices.resize(Base + CharIdx * 4);
	Indices.resize(IdxBase + CharIdx * 6);
}

void FFontBatcher::Clear()
{
	Vertices.clear();
	Indices.clear();
}

void FFontBatcher::Flush(ID3D11DeviceContext* Context, const FFontResource* Resource)
{
	if (!Resource || !Resource->IsLoaded()) return;
	if (Vertices.empty() || !VertexBuffer || !IndexBuffer) return;

	// Atlas 그리드가 바뀌었으면 CharInfoMap 재빌드
	if (CachedColumns != Resource->Columns || CachedRows != Resource->Rows)
	{
		BuildCharInfoMap(Resource->Columns, Resource->Rows);
	}

	// 버퍼 크기 초과 시 재할당
	if (Vertices.size() > MaxVertexCount || Indices.size() > MaxIndexCount)
	{
		MaxVertexCount = static_cast<uint32>(Vertices.size()) * 2;
		MaxIndexCount = static_cast<uint32>(Indices.size()) * 2;
		CreateBuffers();
	}

	// VB 업로드
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(Context->Map(VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
	memcpy(mapped.pData, Vertices.data(), sizeof(FTextureVertex) * Vertices.size());
	Context->Unmap(VertexBuffer, 0);

	// IB 업로드
	if (FAILED(Context->Map(IndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
	memcpy(mapped.pData, Indices.data(), sizeof(uint32) * Indices.size());
	Context->Unmap(IndexBuffer, 0);

	// 셰이더 바인딩
	FontShader.Bind(Context);

	uint32 stride = sizeof(FTextureVertex), offset = 0;
	Context->IASetVertexBuffers(0, 1, &VertexBuffer, &stride, &offset);
	Context->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ResourceManager 소유 SRV 바인딩
	ID3D11ShaderResourceView* SRV = Resource->SRV;
	Context->PSSetShaderResources(0, 1, &SRV);
	Context->PSSetSamplers(0, 1, &SamplerState);

	Context->DrawIndexed(static_cast<uint32>(Indices.size()), 0, 0);
}

void FFontBatcher::GetCharUV(uint32 Codepoint, FVector2& OutUVMin, FVector2& OutUVMax) const
{
	const auto It = CharInfoMap.find(Codepoint);
	if (It == CharInfoMap.end())
	{
		OutUVMin = FVector2(0, 0);
		OutUVMax = FVector2(0, 0);
		return;
	}

	const FCharacterInfo& Info = It->second;
	OutUVMin = FVector2(Info.U, Info.V);
	OutUVMax = FVector2(Info.U + Info.Width, Info.V + Info.Height);
}
