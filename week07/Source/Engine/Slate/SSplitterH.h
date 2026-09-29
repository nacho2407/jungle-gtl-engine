#pragma once
#include "SSplitter.h"
/*
* 수평 분할 Splitter
*/
class SSplitterH : public SSplitter
{
public:
	void  UpdateChildRect()                       override;

	// X 축 기준: 마우스 X 위치 → 좌우 분할 비율
	float ComputeNewRatio(int32 X, int32 Y) const override;

	// 세로 바의 FRect 반환 (X: 바 중앙 기준, Height: 스플리터 전체)
	FRect GetBarRect() const override;
};
