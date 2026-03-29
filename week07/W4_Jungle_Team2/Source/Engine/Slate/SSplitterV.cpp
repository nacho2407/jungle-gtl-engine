#include "SSplitterV.h"

void SSplitterV::UpdateCildRect()
{
	if (!GetSideLT() || !GetSideRB()) return;

	// SSplitterV: 수직 배치 (위/아래 분할, 바는 가로선)
	// 바 두께의 절반씩 위/아래 자식에서 빼서 실제 클릭 가능한 바 영역을 만듭니다.
	FRect R = GetRect();
	const float HalfBar = GetBarThickness() * 0.5f;
	float SplitY = R.Y + R.Height * GetSplitRatio();

	GetSideLT()->SetRect({ R.X, R.Y,              R.Width, SplitY - R.Y - HalfBar              });
	GetSideRB()->SetRect({ R.X, SplitY + HalfBar, R.Width, R.Y + R.Height - SplitY - HalfBar   });

	// 자식이 SSplitter라면 재귀 (SSplitter가 아니라면 빈 함수 출력)
	GetSideLT()->UpdateCildRect();
	GetSideRB()->UpdateCildRect();
}

FRect SSplitterV::GetBarRect() const
{
	const FRect R = GetRect();
	const float HalfBar = GetBarThickness() * 0.5f;
	const float SplitY  = R.Y + R.Height * GetSplitRatio();
	return FRect(R.X, SplitY - HalfBar, R.Width, GetBarThickness());
}

float SSplitterV::ComputeNewRatio(int32 X, int32 Y) const
{
	(void)X;
	const FRect& R = GetRect();
	if (R.Height <= 0.f) return GetSplitRatio();

	float Ratio = (static_cast<float>(Y) - R.Y) / R.Height;
	if (Ratio < 0.05f) Ratio = 0.05f;
	if (Ratio > 0.95f) Ratio = 0.95f;
	return Ratio;
}
