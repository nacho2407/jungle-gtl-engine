#include "SSplitterH.h"

void SSplitterH::UpdateChildRect()
{
	if (!GetSideLT() || !GetSideRB()) return;

	// SSplitterH: 수평 배치 (좌/우 분할, 바는 세로선)
	// 바 두께의 절반씩 좌/우 자식에서 빼서 실제 클릭 가능한 바 영역을 만듭니다.
	FRect R = GetRect();
	const float HalfBar = GetBarThickness() * 0.5f;
	float SplitX = R.X + R.Width * GetSplitRatio();

	GetSideLT()->SetRect({ R.X,            R.Y, SplitX - R.X - HalfBar,              R.Height });
	GetSideRB()->SetRect({ SplitX + HalfBar, R.Y, R.X + R.Width - SplitX - HalfBar, R.Height });

	// 자식이 SSplitter라면 재귀 (SSplitter가 아니라면 빈 함수 출력)
	GetSideLT()->UpdateChildRect();
	GetSideRB()->UpdateChildRect();
}

FRect SSplitterH::GetBarRect() const
{
	const FRect R = GetRect();
	const float HalfBar = GetBarThickness() * 0.5f;
	const float SplitX  = R.X + R.Width * GetSplitRatio();
	return FRect(SplitX - HalfBar, R.Y, GetBarThickness(), R.Height);
}

float SSplitterH::ComputeNewRatio(int32 X, int32 Y) const
{
	(void)Y;
	const FRect& R = GetRect();
	if (R.Width <= 0.f) return GetSplitRatio();

	float Ratio = (static_cast<float>(X) - R.X) / R.Width;
	if (Ratio < 0.05f) Ratio = 0.05f;
	if (Ratio > 0.95f) Ratio = 0.95f;
	return Ratio;
}
