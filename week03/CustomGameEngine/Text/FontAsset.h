#pragma once

#include "EngineCore/EngineTypes.h"
#include "Text/FontTypes.h"
#include "ImGui/imstb_truetype.h"

/**
 * @brief 하나의 폰트에 대한 Asset 객체
 */
class FFontAsset
{
public:
	/**
	 * @brief Glyph 정보 캐시에서 Codepoint에 해당하는 glyph 정보를 가져옵니다.
	 * 
	 * @param Codepoint Atlas에서 찾고자하는 UTF-32 codepoint
	 * 
	 * @return Codepoint에 해당하는 glyph 정보 구조체
	 */
	const FGlyphInfo* FindGlyph(uint32 Codepoint) const;

	/**
	 * @brief Glyph 정보 캐시에서 Codepoint에 해당하는 glyph 정보가 있는지 확인합니다.
	 * 
	 * @param Codepoint Glyph 정보 캐시에서 찾고자하는 UTF-32 codepoint
	 * 
	 * @return Codepoint에 해당하는 Glyph 정보 캐시 존재 여부
	 */
	bool HasGlyph(uint32 Codepoint) const;

	/**
	 * @brief FontPath의 파일로부터 폰트 데이터를 메모리에 올리고 폰트 정보 초기화
	 * 
	 * @param FontPath 불러올 폰트 파일의 위치
	 * 
	 * @param PixelHeight 생성할 glyph의 픽셀 높이 값. PixelWidth는 PixelHeight과 폰트 내 비율 값에 따라 자동 결정됩니다.
	 * 
	 * @bool 파일 불러오기 성공 여부
	 */
	bool LoadFromFile(const FString& FontPath, float PixelHeight);

	/**
	 * @brief Glyph 정보 캐시에 Codepoint에 해당하는 glyph 정보가 존재하는지 확인하고, 없다면 폰트 파일에서 찾아 등록합니다.
	 * 
	 * @param Codepoint Glyph 정보 캐시에서 정보가 존재하는지 확인할 UTF-32 Codepoint 정보
	 * 
	 * @return Codepoint에 해당하는 Glyph 정보
	 */
	FGlyphInfo& EnsureGlyph(uint32 Codepoint);

private:
	/**
	 * @brief 폰트 glyph 정보 캐시 맵
	 */
	TMap<uint32, FGlyphInfo> GlyphMap;

	/**
	 * @brief 파일로부터 불러온 폰트 데이터
	 * 
	 *        std_truetype이 내부적으로 파일 메모리를 직접 참조하기 때문에 lifetime 동안 살아있어야 함
	 */
	TArray<uint8> FontData;

	/**
	 * @brief 폰트 정보 구조체
	 */
	stbtt_fontinfo FontInfo;

	/**
	 * @brief 생성할 glyph의 픽셀 높이 값
	 */
	float FontPixelHeight = 0.0f;

	/**
	 * @brief 폰트 파일 로드 여부
	 */
	bool bLoaded = false;
};
