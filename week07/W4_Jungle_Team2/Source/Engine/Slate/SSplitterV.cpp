#include "SSplitterV.h"

void SSplitterV::UpdateCildRect()
{
	if (!GetSideLT() || !GetSideRB()) return;

	// SSplitterV: 수직 배치 (위/아래 분할, 바는 가로선)
	FRect R = GetRect();
	float SplitY = R.Y + R.Height * GetSplitRatio();

	GetSideLT()->SetRect({ R.X, R.Y,     R.Width, SplitY - R.Y              });
	GetSideRB()->SetRect({ R.X, SplitY,  R.Width, R.Y + R.Height - SplitY   });

	// 자식이 SSplitter라면 재귀 (SSplitter가 아니라면 빈 함수 출력)
	GetSideLT()->UpdateCildRect();
	GetSideRB()->UpdateCildRect();
}
