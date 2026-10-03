#pragma once
#include "SSplitter.h"

class SSplitterCross;

/*
* 수직 분할 Splitter
*/
class SSplitterV : public SSplitter
{
public:
	void  UpdateChildRect()                       override;

	// Y 축 기준: 마우스 Y 위치 → 상하 분할 비율
	float ComputeNewRatio(int32 X, int32 Y) const override;

	// 가로 바의 FRect 반환 (Y: 바 중앙 기준, Width: 스플리터 전체)
	FRect GetBarRect() const override;

	// CrossWidget 을 자식보다 먼저 HitTest 합니다.
	SWidget* HitTest(int32 X, int32 Y) override;

	SSplitterCross* GetCrossWidget() const { return CrossWidget; }
	void SetCrossWidget(SSplitterCross* InCross) { CrossWidget = InCross; }

private:
	SSplitterCross* CrossWidget = nullptr;
};
