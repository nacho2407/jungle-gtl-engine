#pragma once

/**
 * @brief 배치 결과 GPU 정점 데이터
 */
struct FVertexFont
{
	float anchorX = 0.0f, anchorY = 0.0f, anchorZ = 0.0f; // 문자열이 붙어있을 객체 위치
	float offsetX = 0.0f, offsetY = 0.0f; // anchor로부터 떨어진 위치
	float u = 0.0f, v = 0.0f; // Texture Coordinates
};
