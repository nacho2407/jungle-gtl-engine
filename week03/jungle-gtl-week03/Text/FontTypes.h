#pragma once

#include "EngineCore/EngineTypes.h"

/**
 * @brief Atlas 내 픽셀 영역
 */
struct FAtlasRect
{
	float X = 0.0f, Y = 0.0f; // Atlas 내 시작 지점
	float Width = 0.0f, Height = 0.0f; // Atlas 내 Glyph 크기
};

/**
 * @brief Glyph의 atlas 내 기하학적 배치 정보
 */
struct FGlyphMetrics
{
	float AdvanceX = 0.0f; // 해당 glyph를 배치한 뒤 다음 글자를 위해 이동할 거리
	float BearingX = 0.0f, BearingY = 0.0f; // 현재 펜 위치에서 glyph bitmap의 시작점까지의 오프셋
	float Width = 0.0f, Height = 0.0f; // Padding, trim 등을 고려한 Glyph의 크기
};

/**
 * @brief Codepoint 하나에 대한 최종 캐시 정보
 */
struct FGlyphInfo
{
	uint32 Codepoint = 0; // 해당 글자의 UTF-32 변환 정보
	FAtlasRect AtlasRect{};
	FGlyphMetrics Metrics{};

	// 나눗셈 연산을 줄이기 위해 추가
	float U0 = 0.0f, V0 = 0.0f; // Atlas Bitmap 내 정규화된 시작 좌표
	float U1 = 0.0f, V1 = 0.0f; // Atlas Bitmap 내 정규화된 끝 좌표
};

/**
 * @brief 임시 비트맵 저장 구조체
 */
struct FGlyphBitmap
{
	int Width = 0, Height = 0; // 생성된 비트맵 크기
	int OffsetX = 0, OffsetY = 0; // stb_truetype에서 자동 생성되는 bearing 계열 오프셋
	TArray<uint8> Pixels; // 8-bit alpha 비트맵
};
