#include "SSplitterH.h"

void SSplitterH::UpdateCildRect()
{
	if (!GetSideLT() || !GetSideRB()) return;

	// SSplitterH: 수평 배치 (좌/우 분할, 바는 세로선)
	FRect R = GetRect();
	float SplitX = R.X + R.Width * GetSplitRatio();

	GetSideLT()->SetRect({ R.X,    R.Y, SplitX - R.X,           R.Height });
	GetSideRB()->SetRect({ SplitX, R.Y, R.X + R.Width - SplitX, R.Height });
	
	// 자식이 SSplitter라면 재귀 (SSplitter가 아니라면 빈 함수 출력)
	GetSideLT()->UpdateCildRect();
	GetSideRB()->UpdateCildRect();
}
