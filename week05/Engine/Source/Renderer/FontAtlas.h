#pragma once

#include "CoreMinimal.h"
#include <d3d11.h>

struct ENGINE_API FFontGlyph
{
	float U0 = 0.0f;
	float V0 = 0.0f;
	float U1 = 0.0f;
	float V1 = 0.0f;

	float Width = 1.0f;
	float Height = 1.0f;
	float Advance = 1.0f;
};

struct ENGINE_API FAtlasRange
{
	uint32 StartCodepoint = 0;
	uint32 Count = 0;
	uint32 StartIndex = 0;
};

class ENGINE_API FFontAtlas
{
public:
	FFontAtlas() = default;
	~FFontAtlas();

	bool Initialize(
		ID3D11Device* Device,
		ID3D11DeviceContext* DeviceContext,
		const std::wstring& TexturePath
	);

	void Release();

	const FFontGlyph& GetGlyph(uint32 Codeint) const;

	ID3D11ShaderResourceView* GetTextureSRV() const { return TextureSRV; }
	ID3D11SamplerState* GetSamplerState() const { return SamplerState; }

	static constexpr uint32 CellsPerRow = 128;
	static constexpr uint32 Rows = 128;
	static constexpr uint32 GlyphCount = CellsPerRow * Rows;
	static constexpr uint32 BaseCodepoint = 0x0020;

private:
	void BuildGridAtlas();

private:
	ID3D11ShaderResourceView* TextureSRV = nullptr;
	ID3D11SamplerState* SamplerState = nullptr;

	FFontGlyph Glyphs[GlyphCount] = {};

	FAtlasRange AskiiRange;
	FAtlasRange JamoRange;
	FAtlasRange KRRange;
};