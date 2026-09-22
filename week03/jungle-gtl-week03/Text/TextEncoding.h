#pragma once

#include "EngineCore/EngineTypes.h"
#include "Text/FontConfig.h"

/**
 * @brief 가변 길이 UTF-8로 되어있는 FString(std::string) 문자열을 디코딩하여 고정 길이 UTF-32 문자열로 반환합니다.
 * 
 *        동적 Glype Atlas 구현 시 Glype Lookup에 필요합니다.
 * 
 * @param Text 변환할 FString 문자열
 * 
 * @return 변환된 UTF-32 문자열
 */
FU32String Utf8ToCodepoints(const FString& Text);
