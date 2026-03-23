#include "Text/TextLayout.h"

#include "EngineCore/Logger.h"
#include "Text/TextEncoding.h"

bool BuildBillboardTextVertices(const FTextLayoutBuildParams& Params, TArray<FVertexFont>& OutVertices)
{
	if (Params.FontAsset == nullptr || Params.Scale <= 0.0f)
	{
		UE_LOG("[Text/TextLayout][Error] Invalid layout build params");

		return false;
	}

	OutVertices.clear();

	if (Params.Text.empty())
	{
		// 빈 문자열은 성공 처리
		return true;
	}

	FU32String Codepoints = Utf8ToCodepoints(Params.Text);
	if (Codepoints.empty())
	{
		return true;
	}

	OutVertices.reserve(Codepoints.size() * 6);

	float PenX = 0.0f;
	float PenY = 0.0f;

	for (uint32 Codepoint: Codepoints)
	{
		// 개행 처리
		if (Codepoint == U'\n')
		{
			PenX = 0.0f;
			PenY -= Params.FontAsset->GetFontPixelHeight() * Params.Scale + Params.LineGap;

			continue;
		}

		// 필요 glyph 확보
		const FGlyphInfo* Glyph = Params.FontAsset->EnsureGlyph(Codepoint);
		if (Glyph == nullptr)
		{
			return false;
		}

		// 공백 처리. 쿼드를 만들지 않고 Advance만 적용
		if (Glyph->Metrics.Width <= 0.0f || Glyph->Metrics.Height <= 0.0f)
		{
			PenX += Glyph->Metrics.AdvanceX * Params.Scale;
			continue;
		}

		// glyph 쿼드의 로컬 오프셋 계산
		float GlyphX0 = PenX + Glyph->Metrics.BearingX * Params.Scale;
		float GlyphX1 = GlyphX0 + Glyph->Metrics.Width * Params.Scale;
		float GlyphY1 = PenY + Glyph->Metrics.BearingY * Params.Scale;
		float GlyphY0 = GlyphY1 - Glyph->Metrics.Height * Params.Scale;

		// UV 읽기
		float U0 = Glyph->U0;
		float V0 = Glyph->V0;
		float U1 = Glyph->U1;
		float V1 = Glyph->V1;

		// 쿼드를 위한 정점 6개(삼각형 2개) 추가
		OutVertices.push_back()

		// 다음 글자 위치로 Pen 이동
		PenX += Glyph->Metrics.AdvanceX * Params.Scale + LETTER_QUAD_SPACE;
	}

	return true;
}
