#pragma once
#include "SWidget.h"

class SSplitterV;
class SSplitterH;

/*
 * 4분할 뷰포트 중앙 교차점 핸들 위젯
 * SSplitterV(가로 바)와 SSplitterH(세로 바)가 겹치는 영역을 점유합니다.
 * 드래그 시 SSplitterV(상하)와 SSplitterH(좌우) 비율을 동시에 조정합니다.
 */
class SSplitterCross : public SWidget
{
public:
    void SetSplitterV(SSplitterV* InV) { SplitterV = InV; }
    void SetSplitterH(SSplitterH* InH) { SplitterH = InH; }

    // SSplitterV 가로 바 ∩ SSplitterH 세로 바 의 교차 영역을 반환합니다.
    FRect GetCrossRect() const;

    SWidget* HitTest(int32 X, int32 Y) override;

    bool OnMouseButtonDown(int32 Button, int32 X, int32 Y) override;
    bool OnMouseMove(int32 X, int32 Y) override;
    bool OnMouseButtonUp(int32 Button, int32 X, int32 Y) override;    

    bool IsDragging() const { return bDragging; }

private:
    SSplitterV* SplitterV = nullptr;
    SSplitterH* SplitterH = nullptr;  // TopSplitterH (BotSplitterH 와 linked)

    bool bDragging = false;
};
