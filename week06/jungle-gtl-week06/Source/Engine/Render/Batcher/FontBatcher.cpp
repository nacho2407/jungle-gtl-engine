#include "FontBatcher.h"

#include "Core/CoreTypes.h"
#include "Profiling/Stats.h"
#include "Resource/ResourceManager.h"
#include "Render/Resource/ShaderManager.h"

void FFontBatcher::Create(ID3D11Device* InDevice)
{
	CreateBuffers(InDevice, 1024, sizeof(FTextureVertex), 1536);
	if (!Device) return;

	// Sampler — Point 필터 (폰트는 선명하게)
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	Device->CreateSamplerState(&sampDesc, &SamplerState);

	if (const FFontResource* DefaultFont = FResourceManager::Get().FindFont(FName("Default")))
	{
		if (DefaultFont->Columns > 0 && DefaultFont->Rows > 0)
		{
			BuildCharInfoMap(DefaultFont->Columns, DefaultFont->Rows);
		}
	}
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
	ClearScreen();

	if (SamplerState) { SamplerState->Release(); SamplerState = nullptr; }

	ReleaseBuffers();
}

void FFontBatcher::AddText(const FString& Text,
	const FVector& WorldPos,
	const FVector& CamRight,
	const FVector& CamUp,
	const FVector& WorldScale,
	float Scale)
{
	if (Text.empty()) return;

	const float CharW = 0.5f * Scale * WorldScale.Y;
	const float CharH = 0.5f * Scale * WorldScale.Z;
	float CharCursorX = 0.0f;
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


// 오버레이 텍스트
void FFontBatcher::AddScreenText(const FString& Text,
	float ScreenX, float ScreenY,
	float ViewportWidth, float ViewportHeight,
	float Scale)
{
	if (Text.empty())
	{
		return;
	}

	if (ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
	{
		return;
	}

	const float CharW = 23.0f * Scale;
	const float CharH = 23.0f * Scale;
	const float LetterSpacing = - 0.5f * CharW;

	const uint32 Base = static_cast<uint32>(ScreenVertices.size());
	const uint32 IdxBase = static_cast<uint32>(ScreenIndices.size());
	const size_t CharCount = Text.size();

	ScreenVertices.resize(Base + CharCount * 4);
	ScreenIndices.resize(IdxBase + CharCount * 6);

	FTextureVertex* pV = ScreenVertices.data() + Base;
	uint32* pI = ScreenIndices.data() + IdxBase;

	const uint8* Ptr = reinterpret_cast<const uint8*>(Text.c_str());
	const uint8* const End = Ptr + Text.size();

	uint32 CharIdx = 0;
	float CursorX = ScreenX;

	auto PixelToClipX = [ViewportWidth](float X) -> float
		{
			return (X / ViewportWidth) * 2.0f - 1.0f;
		};

	auto PixelToClipY = [ViewportHeight](float Y) -> float
		{
			return 1.0f - (Y / ViewportHeight) * 2.0f;
		};

	for (size_t i = 0; i < CharCount && Ptr < End; ++i)
	{
		uint32 CP = 0;
		if (Ptr[0] < 0x80) { CP = Ptr[0];                                                                       Ptr += 1; }
		else if ((Ptr[0] & 0xE0) == 0xC0 && Ptr + 1 < End) { CP = ((Ptr[0] & 0x1F) << 6) | (Ptr[1] & 0x3F);                                   Ptr += 2; }
		else if ((Ptr[0] & 0xF0) == 0xE0 && Ptr + 2 < End) { CP = ((Ptr[0] & 0x0F) << 12) | ((Ptr[1] & 0x3F) << 6) | (Ptr[2] & 0x3F);         Ptr += 3; }
		else if ((Ptr[0] & 0xF8) == 0xF0 && Ptr + 3 < End) { CP = ((Ptr[0] & 0x07) << 18) | ((Ptr[1] & 0x3F) << 12) | ((Ptr[2] & 0x3F) << 6) | (Ptr[3] & 0x3F); Ptr += 4; }
		else { ++Ptr; continue; }

		FVector2 UVMin, UVMax;
		GetCharUV(CP, UVMin, UVMax);

		const float Left = PixelToClipX(CursorX);
		const float Right = PixelToClipX(CursorX + CharW);
		const float Top = PixelToClipY(ScreenY);
		const float Bottom = PixelToClipY(ScreenY + CharH);

		pV[0] = { FVector(Left,  Top,    0.0f), FVector2(UVMin.X, UVMin.Y) };
		pV[1] = { FVector(Right, Top,    0.0f), FVector2(UVMax.X, UVMin.Y) };
		pV[2] = { FVector(Left,  Bottom, 0.0f), FVector2(UVMin.X, UVMax.Y) };
		pV[3] = { FVector(Right, Bottom, 0.0f), FVector2(UVMax.X, UVMax.Y) };

		const uint32 Vi = Base + CharIdx * 4;
		pI[0] = Vi;
		pI[1] = Vi + 1;
		pI[2] = Vi + 2;
		pI[3] = Vi + 1;
		pI[4] = Vi + 3;
		pI[5] = Vi + 2;

		pV += 4;
		pI += 6;
		++CharIdx;

		CursorX += CharW + LetterSpacing;
	}

	ScreenVertices.resize(Base + CharIdx * 4);
	ScreenIndices.resize(IdxBase + CharIdx * 6);
}

void FFontBatcher::Clear()
{
	Vertices.clear();
	Indices.clear();
}

void FFontBatcher::ClearScreen()
{
	ScreenVertices.clear();
	ScreenIndices.clear();
}

void FFontBatcher::DrawBatch(ID3D11DeviceContext* Context, const FFontResource* Resource)
{
	if (!Resource || !Resource->IsLoaded()) return;
	if (Vertices.empty()) return;

	if (CachedColumns != Resource->Columns || CachedRows != Resource->Rows)
	{
		BuildCharInfoMap(Resource->Columns, Resource->Rows);
	}

	const uint32 VertexCount = static_cast<uint32>(Vertices.size());
	const uint32 IndexCount = static_cast<uint32>(Indices.size());

	VB.EnsureCapacity(Device, VertexCount);
	IB.EnsureCapacity(Device, IndexCount);
	if (!VB.Update(Context, Vertices.data(), VertexCount)) return;
	if (!IB.Update(Context, Indices.data(), IndexCount)) return;

	FShaderManager::Get().GetShader(EShaderType::Font)->Bind(Context);
	VB.Bind(Context);
	IB.Bind(Context);

	ID3D11ShaderResourceView* SRV = Resource->SRV;
	Context->PSSetShaderResources(0, 1, &SRV);
	Context->PSSetSamplers(0, 1, &SamplerState);

	Context->DrawIndexed(IndexCount, 0, 0);
	FDrawCallStats::Increment();
}

void FFontBatcher::DrawScreenBatch(ID3D11DeviceContext* Context, const FFontResource* Resource)
{
	if (!Resource || !Resource->IsLoaded()) return;
	if (ScreenVertices.empty()) return;

	if (CachedColumns != Resource->Columns || CachedRows != Resource->Rows)
	{
		BuildCharInfoMap(Resource->Columns, Resource->Rows);
	}

	const uint32 VertexCount = static_cast<uint32>(ScreenVertices.size());
	const uint32 IndexCount = static_cast<uint32>(ScreenIndices.size());

	VB.EnsureCapacity(Device, VertexCount);
	IB.EnsureCapacity(Device, IndexCount);
	if (!VB.Update(Context, ScreenVertices.data(), VertexCount)) return;
	if (!IB.Update(Context, ScreenIndices.data(), IndexCount)) return;

	FShaderManager::Get().GetShader(EShaderType::OverlayFont)->Bind(Context);
	VB.Bind(Context);
	IB.Bind(Context);

	ID3D11ShaderResourceView* SRV = Resource->SRV;
	Context->PSSetShaderResources(0, 1, &SRV);
	Context->PSSetSamplers(0, 1, &SamplerState);

	Context->DrawIndexed(IndexCount, 0, 0);
	FDrawCallStats::Increment();
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
