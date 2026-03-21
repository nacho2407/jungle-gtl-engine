#pragma once

#include "EngineCore/EngineTypes.h"

/**
 * @brief Atlas 내 픽셀 영역
 */
struct FAtlasRect
{
	float X, Y; // Atlas 내 시작 지점
	float Width, Height; // Atlas 내 Glyph 크기
};

/**
 * @brief 폰트 metric 정보
 */
struct FGlyphMetrics
{
	float AdvanceX; // 해당 glyph를 배치한 뒤 다음 글자를 위해 이동할 거리
	float BearingX, BearingY; // 현재 펜 위치에서 glyph bitmap의 시작점까지의 오프셋
	float Width, Height; // Padding, trim 등을 고려한 Glyph의 크기
};

/**
 * @brief Codepoint 하나에 대한 최종 캐시 정보
 */
struct FGlyphInfo
{
	uint32 Codepoint; // 해당 글자의 UTF-32 변환 정보
	FAtlasRect AtlasRect;
	FGlyphMetrics Metrics;

	// 나눗셈 연산을 줄이기 위해 추가
	float U0, V0; // Atlas Bitmap 내 정규화된 시작 좌표
	float U1, V1; // Atlas Bitmap 내 정규화된 끝 좌표
};
