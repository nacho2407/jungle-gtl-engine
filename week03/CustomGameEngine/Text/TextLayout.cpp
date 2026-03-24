#include "Text/TextLayout.h"

#include "EngineCore/Logger.h"
#include "Text/TextEncoding.h"

bool AppendBillboardTextVertices(const FTextLayoutBuildParams& Params, TArray<FVertexFont>& InOutVertices)
{
	if (Params.FontAsset == nullptr || Params.Scale <= 0.0f)
	{
		UE_LOG("[Text/TextLayout][Error] Invalid layout build params");

		return false;
	}

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

	InOutVertices.reserve(InOutVertices.size() + Codepoints.size() * 6);

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
			UE_LOG("[Text/TextLayout][Error] Failed to get necessary glyph");
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
		// Clockwise front-face assumed
		// 좌상
		InOutVertices.push_back({
			Params.Anchor.x, Params.Anchor.y, Params.Anchor.z,
			GlyphX0, GlyphY1,
			U0, V0
		});

		// 우상
		InOutVertices.push_back({
			Params.Anchor.x, Params.Anchor.y, Params.Anchor.z,
			GlyphX1, GlyphY1,
			U1, V0
			});

		// 좌하
		InOutVertices.push_back({
			Params.Anchor.x, Params.Anchor.y, Params.Anchor.z,
			GlyphX0, GlyphY0,
			U0, V1
		});

		// 우상
		InOutVertices.push_back({
			Params.Anchor.x, Params.Anchor.y, Params.Anchor.z,
			GlyphX1, GlyphY1,
			U1, V0
		});

		// 우하
		InOutVertices.push_back({
			Params.Anchor.x, Params.Anchor.y, Params.Anchor.z,
			GlyphX1, GlyphY0,
			U1, V1
		});

		// 좌하
		InOutVertices.push_back({
			Params.Anchor.x, Params.Anchor.y, Params.Anchor.z,
			GlyphX0, GlyphY0,
			U0, V1
		});

		// 다음 글자 위치로 Pen 이동
		PenX += Glyph->Metrics.AdvanceX * Params.Scale;
	}

	return true;
}
