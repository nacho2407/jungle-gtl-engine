#pragma once

struct FVertexFont
{
	// 빌보드 폰트의 경우에는 어차피 배치 렌더링을 별도의 함수로 할 예정이기 때문에 방식을 조금 다르게 갑니다.
	// 빌보드 폰트는 특정 메쉬에 붙어있어야하기 때문에 셰이더에 전달 시 자기가 붙어있을 객체의 위치, 그 위치로부터 떨어진 거리, 텍스처 UV 좌표만 전달합니다.
	float anchorX, anchorY, anchorZ; // 문자열이 붙어있을 객체 위치
	float offsetX, offsetY; // anchor로부터 떨어진 위치
	float u, v; // Texture Coordinates
};
