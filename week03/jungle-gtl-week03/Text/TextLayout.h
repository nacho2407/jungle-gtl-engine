#pragma once

#include "EngineCore/EngineTypes.h"
#include "Math/Vector.h"
#include "Renderer/VertexFont.h"
#include "Text/FontAsset.h"
#include "Text/FontConfig.h"

/**
 * @brief 텍스트의 Quad 레이아웃을 설명하는 구조체
 */
struct FTextLayoutBuildParams
{
	FFontAsset* FontAsset = nullptr; // 어떤 폰트 데이터를 사용하는지
	FString Text; // 실제 배치할 문자열 내용
	FVector Anchor{}; // 텍스트 전체 배치의 시작 기준점
	float Scale = 0.0f; // Glyph 크기 배율
	float LineGap = 0.0f; // 줄바꿈 시 줄 간격
};

/**
 * @brief 입력으로 받은 레이아웃을 GPU 정점으로 생성하여 텍스트 Quad 정점 배열에 추가합니다.
 * 
 * @param Params 텍스트 Quad 정점 배열에 추가할 입력 레이아웃
 * 
 * @param InOutVertices 새 텍스트 Quad 정점을 추가할 배열
 * 
 * @return 정점 생성 및 추가 성공 여부
 */
bool AppendBillboardTextVertices(const FTextLayoutBuildParams& Params, TArray<FVertexFont>& InOutVertices);
