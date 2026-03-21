#pragma once

#include "EngineCore/EngineTypes.h"
#include "Text/FontTypes.h"

/**
 * @brief 폰트로부터 제작한 glyph 비트맵을 붙여넣는 atlas
 */
class FGlyphAtlas
{
public:
	/**
	 * @brief Atlas를 초기화합니다.
	 * 
	 * @param Width Atlas의 너비
	 * 
	 * @param Height Atlas의 높이
	 * 
	 * @return 초기화 성공 여부
	 */
	bool Initialize(uint32 Width, uint32 Height);

	/**
	 * @brief Atlas에 glyph 비트맵을 추가합니다.
	 * 
	 * @param Bitmap Atlas에 추가할 비트맵
	 * 
	 * @param OutRect 추가된 비트맵의 atlas 내 정보
	 * 
	 * @return 추가 성공 여부
	 */
	bool AddGlyphBitmap(const FGlyphBitmap& Bitmap, FAtlasRect& OutRect);

private:
	uint32 AltasWidth = 0, AtlasHeight = 0; // Atlas 크기

	TArray<uint8> Pixels; // Atlas 픽셀 데이터

	uint32 CursorX = 0, CursorY = 0; // 다음 glyph를 넣기 시작할 위치
	uint32 CurrentRowHeight = 0; // 현재 행에 들어간 glyph들 중 가장 큰 높이
};
