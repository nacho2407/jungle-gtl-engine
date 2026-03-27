#include "FontAtlas.h"
#include <WICTextureLoader.h>
#include <fstream>
#include <filesystem>
#include <Windows.h>

FFontAtlas::~FFontAtlas()
{
	Release();
}

bool FFontAtlas::Initialize(
	ID3D11Device* Device,
	ID3D11DeviceContext* DeviceContext,
	const std::wstring& TexturePath)
{
	Release();

	if (!Device || !DeviceContext || TexturePath.empty())
	{
		MessageBox(0, L"FontAtlas: invalid Device / DeviceContext / TexturePath", 0, 0);
		return false;
	}

	std::ifstream TestFile(std::filesystem::path(TexturePath), std::ios::binary);
	if (!TestFile.is_open())
	{
		MessageBox(0, TexturePath.c_str(), L"FontAtlas: std::ifstream open failed", 0);
		return false;
	}
	TestFile.close();

	if (!std::filesystem::exists(TexturePath)) {
		MessageBox(0, TexturePath.c_str(), L"FontAtlas: File does not exist", 0);
		return false;
	}

	HRESULT Hr = DirectX::CreateWICTextureFromFile(
		Device,
		DeviceContext,
		TexturePath.c_str(),
		nullptr,
		&TextureSRV
	);

	if (FAILED(Hr) || !TextureSRV)
	{
		wchar_t Buffer[256];
		swprintf_s(Buffer, L"FontAtlas: CreateWICTextureFromFile failed\nHRESULT = 0x%08X", Hr);
		MessageBox(0, Buffer, L"FontAtlas Error", 0);
		return false;
	}

	D3D11_SAMPLER_DESC SampDesc = {};
	SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SampDesc.MipLODBias = 0.0f;
	SampDesc.MaxAnisotropy = 1;
	SampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SampDesc.BorderColor[0] = 0.0f;
	SampDesc.BorderColor[1] = 0.0f;
	SampDesc.BorderColor[2] = 0.0f;
	SampDesc.BorderColor[3] = 0.0f;
	SampDesc.MinLOD = 0.0f;
	SampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	Hr = Device->CreateSamplerState(&SampDesc, &SamplerState);
	if (FAILED(Hr) || !SamplerState)
	{
		MessageBox(0, L"FontAtlas: CreateSamplerState failed", 0, 0);
		Release();
		return false;
	}

	BuildGridAtlas();
	return true;
}

// 매핑
void FFontAtlas::BuildGridAtlas()
{
	const float CellU = 1.0f / static_cast<float>(CellsPerRow);
	const float CellV = 1.0f / static_cast<float>(Rows);

	for (uint32 Index = 0; Index < GlyphCount; ++Index)
	{
		const uint32 Row = Index / CellsPerRow;
		const uint32 Col = Index % CellsPerRow;

		FFontGlyph& Glyph = Glyphs[Index];
		Glyph.U0 = static_cast<float>(Col) * CellU;
		Glyph.V0 = static_cast<float>(Row) * CellV;
		Glyph.U1 = static_cast<float>(Col + 1) * CellU;
		Glyph.V1 = static_cast<float>(Row + 1) * CellV;

		Glyph.Width = 1.0f;
		Glyph.Height = 1.0f;
		Glyph.Advance = 1.0f;
	}

	// ASCII: 공백 부터 ~ 까지
	AskiiRange.StartCodepoint = 0x0020;
	AskiiRange.Count = (0x007E - 0x0020 + 1);
	AskiiRange.StartIndex = 0;

	// 호환 자모: ㄱ ~ ㅣ
	// atlas index 96 ~ 146
	JamoRange.StartCodepoint = 0x3131;
	JamoRange.Count = (0x3163 - 0x3131 + 1); // 51
	JamoRange.StartIndex = 96;

	// 완성형 한글: 가 ~ 힣
	KRRange.StartCodepoint = 0xAC00;
	KRRange.Count = (0xD7A3 - 0xAC00 + 1); // 11172
	KRRange.StartIndex = 147;

	// 공백 폭
	const uint32 SpaceIndex = AskiiRange.StartIndex + (' ' - AskiiRange.StartCodepoint);
	if (SpaceIndex < GlyphCount)
	{
		Glyphs[SpaceIndex].Width = 0.0f;
		Glyphs[SpaceIndex].Height = 0.0f;
		Glyphs[SpaceIndex].Advance = 0.35f;
	}
}

// 없는 문자면 ?로 반환
const FFontGlyph& FFontAtlas::GetGlyph(uint32 Codepoint) const
{
	static FFontGlyph EmptyFallback = { 0, 0, 0, 0, 0, 0, 1.0f };

	const auto GetGlyphFromIndex = [&](uint32 Index) -> const FFontGlyph&
		{
			if (Index >= GlyphCount)
			{
				return EmptyFallback;
			}
			return Glyphs[Index];
		};

	const auto GetQuestionGlyph = [&]() -> const FFontGlyph&
		{
			const uint32 QuestionCodepoint = static_cast<uint32>('?');

			if (QuestionCodepoint >= AskiiRange.StartCodepoint &&
				QuestionCodepoint < AskiiRange.StartCodepoint + AskiiRange.Count)
			{
				const uint32 LocalIndex = QuestionCodepoint - AskiiRange.StartCodepoint;
				return GetGlyphFromIndex(AskiiRange.StartIndex + LocalIndex);
			}

			return EmptyFallback;
		};

	if (Codepoint >= AskiiRange.StartCodepoint &&
		Codepoint < AskiiRange.StartCodepoint + AskiiRange.Count)
	{
		const uint32 LocalIndex = Codepoint - AskiiRange.StartCodepoint;
		return GetGlyphFromIndex(AskiiRange.StartIndex + LocalIndex);
	}

	if (Codepoint >= JamoRange.StartCodepoint &&
		Codepoint < JamoRange.StartCodepoint + JamoRange.Count)
	{
		const uint32 LocalIndex = Codepoint - JamoRange.StartCodepoint;
		return GetGlyphFromIndex(JamoRange.StartIndex + LocalIndex);
	}

	if (Codepoint >= KRRange.StartCodepoint &&
		Codepoint < KRRange.StartCodepoint + KRRange.Count)
	{
		const uint32 LocalIndex = Codepoint - KRRange.StartCodepoint;
		return GetGlyphFromIndex(KRRange.StartIndex + LocalIndex);
	}

	return GetQuestionGlyph();
}

void FFontAtlas::Release()
{
	if (SamplerState)
	{
		SamplerState->Release();
		SamplerState = nullptr;
	}

	if (TextureSRV)
	{
		TextureSRV->Release();
		TextureSRV = nullptr;
	}
}