#include <d3d11.h>
#include "FontBatcher.h"
#include "Core/CoreTypes.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/RenderResources.h"

void FFontBatcher::Create(ID3D11Device* InDevice)
{
	Device = InDevice;

	// Dynamic VB/IB 초기 할당 (텍스처는 ResourceManager가 소유 ? 여기서 로드하지 않음)
	MaxVertexCount = 1024;
	MaxIndexCount = 1536;
	CreateBuffers();

	// Sampler ? Point 필터 (폰트는 선명하게)
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	Device->CreateSamplerState(&sampDesc, SamplerState.ReleaseAndGetAddressOf());

	FontMaterial = FResourceManager::Get().GetOrCreateMaterial("FontMaterial", "Shaders/ShaderFont.hlsl");

	UMaterial* Mat = Cast<UMaterial>(FontMaterial);
	Mat->SetParam("FontAtlas", FMaterialParamValue(FResourceManager::Get().LoadTexture("Asset/Font/FontAtlas.dds", Device.Get())));
	Mat->BlendType = EBlendType::AlphaBlend;
	Mat->DepthStencilType = EDepthStencilType::Default;
}

void FFontBatcher::CreateBuffers()
{
	VertexBuffer.Reset();
	IndexBuffer.Reset();

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.Usage = D3D11_USAGE_DYNAMIC;
	vbDesc.ByteWidth = sizeof(FTextureVertex) * MaxVertexCount;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Device->CreateBuffer(&vbDesc, nullptr, VertexBuffer.ReleaseAndGetAddressOf());

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.Usage = D3D11_USAGE_DYNAMIC;
	ibDesc.ByteWidth = sizeof(uint32) * MaxIndexCount;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Device->CreateBuffer(&ibDesc, nullptr, IndexBuffer.ReleaseAndGetAddressOf());
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

	VertexBuffer.Reset();
	IndexBuffer.Reset();
	SamplerState.Reset();
	Device.Reset();
}

void FFontBatcher::AddText(const FString& Text,
	const FMatrix& ModelMatrix,
	float Scale)
{
	if (Text.empty()) return;

	// 1. 실제 유니코드 문자 개수 세기
	const uint8* Ptr = reinterpret_cast<const uint8*>(Text.c_str());
	const uint8* const End = Ptr + Text.size();
	size_t ActualCharCount = 0;
	{
		const uint8* CountPtr = Ptr;
		while (CountPtr < End)
		{
			if (CountPtr[0] < 0x80) { CountPtr += 1; }
			else if ((CountPtr[0] & 0xE0) == 0xC0 && CountPtr + 1 < End) { CountPtr += 2; }
			else if ((CountPtr[0] & 0xF0) == 0xE0 && CountPtr + 2 < End) { CountPtr += 3; }
			else if ((CountPtr[0] & 0xF8) == 0xF0 && CountPtr + 3 < End) { CountPtr += 4; }
			else { ++CountPtr; continue; }
			++ActualCharCount;
		}
	}

	FVector RightVector = ModelMatrix.GetRightVector();
	FVector UpVector = ModelMatrix.GetUpVector();
	FVector WorldScale = ModelMatrix.GetScaleVector();

	const float CharW = 0.5f * Scale * WorldScale.Y;
	const float CharH = 0.5f * Scale * WorldScale.Z;
	float CharCursorX = 0.0f;
	const uint32 Base = static_cast<uint32>(Vertices.size());
	const uint32 IdxBase = static_cast<uint32>(Indices.size());
	const size_t CharCount = Text.size();

	// resize + 포인터 직접 쓰기로 push_back 오버헤드 제거
	Vertices.resize(Base + ActualCharCount * 4);
	Indices.resize(IdxBase + ActualCharCount * 6);
	FTextureVertex* pV = Vertices.data() + Base;
	uint32* pI = Indices.data() + IdxBase;

	// 빌보드 반벡터를 루프 밖에서 미리 계산
	const FVector HalfRight = RightVector * (CharW * 0.5f);
	const FVector HalfUp    = UpVector    * (CharH * 0.5f);

	Ptr = reinterpret_cast<const uint8*>(Text.c_str());
	uint32 CharIdx = 0;

	CharCursorX = -(ActualCharCount * CharW * 0.5f);
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

		const FVector Center = ModelMatrix.GetOrigin() + RightVector * CharCursorX;

		/*pV[0] = { Center - HalfRight + HalfUp, { UVMin.X, UVMin.Y } };
		pV[1] = { Center + HalfRight + HalfUp, { UVMax.X, UVMin.Y } };
		pV[2] = { Center - HalfRight - HalfUp, { UVMin.X, UVMax.Y } };
		pV[3] = { Center + HalfRight - HalfUp, { UVMax.X, UVMax.Y } };*/

		pV[0] = { Center                 + HalfUp, { UVMin.X, UVMin.Y } };
		pV[1] = { Center + HalfRight * 2 + HalfUp, { UVMax.X, UVMin.Y } };
		pV[2] = { Center                 - HalfUp, { UVMin.X, UVMax.Y } };
		pV[3] = { Center + HalfRight * 2 - HalfUp, { UVMax.X, UVMax.Y } };

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

void FFontBatcher::AddText2D(const FString& Text, const FVector2& ScreenPos, float ViewportWidth, float ViewportHeight, float Scale)
{
	if (Text.empty() || ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
	{
		return;
	}

	float CurrentX = ScreenPos.X;
	float CurrentY = ScreenPos.Y;

	for (char C : Text)
	{
		FVector2 UVMin, UVMax;
		GetCharUV(C, UVMin, UVMax);

		float CharWidth = 20.0f * Scale;
        float CharHeight = 20.0f * Scale;

		// Top Left to Bottom Right
		FVector PosTL(CurrentX, CurrentY, 0.0f);
		FVector PosTR(CurrentX + CharWidth, CurrentY, 0.0f);
		FVector PosBL(CurrentX, CurrentY + CharHeight, 0.0f);
		FVector PosBR(CurrentX + CharWidth, CurrentY + CharHeight, 0.0f);

		uint32 StartIndex = static_cast<uint32>(Vertices.size());

		Vertices.push_back({PosTL, FVector2(UVMin.X, UVMin.Y)});
		Vertices.push_back({PosTR, FVector2(UVMax.X, UVMin.Y)});
		Vertices.push_back({PosBL, FVector2(UVMin.X, UVMax.Y)});
		Vertices.push_back({PosBR, FVector2(UVMax.X, UVMax.Y)});

		Indices.push_back(StartIndex + 0);
		Indices.push_back(StartIndex + 1);
		Indices.push_back(StartIndex + 2);

		Indices.push_back(StartIndex + 1);
		Indices.push_back(StartIndex + 3);
		Indices.push_back(StartIndex + 2);

		CurrentX += CharWidth;
	}
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
	if (FAILED(Context->Map(VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
	memcpy(mapped.pData, Vertices.data(), sizeof(FTextureVertex) * Vertices.size());
	Context->Unmap(VertexBuffer.Get(), 0);

	// IB 업로드
	if (FAILED(Context->Map(IndexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
	memcpy(mapped.pData, Indices.data(), sizeof(uint32) * Indices.size());
	Context->Unmap(IndexBuffer.Get(), 0);

	// 셰이더 바인딩
	FontMaterial->Bind(Context);

	uint32 stride = sizeof(FTextureVertex), offset = 0;
	ID3D11Buffer* VertexBufferPtr = VertexBuffer.Get();
	ID3D11Buffer* IndexBufferPtr = IndexBuffer.Get();
	Context->IASetVertexBuffers(0, 1, &VertexBufferPtr, &stride, &offset);
	Context->IASetIndexBuffer(IndexBufferPtr, DXGI_FORMAT_R32_UINT, 0);
	Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11SamplerState* Samplers[] = { SamplerState.Get() };
	Context->PSSetSamplers(0, 1, Samplers);

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

