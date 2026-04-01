#include "SSplitter.h"
#include "SlateApplication.h"

SWidget* SSplitter::HitTest(int32 X, int32 Y)
{
	// 자신의 영역 밖이면 즉시 반환
	if (!GetRect().Contains(static_cast<float>(X), static_cast<float>(Y)))
		return nullptr;

	// SideLT → SideRB 순으로 자식 검사
	if (SideLT)
	{
		SWidget* Hit = SideLT->HitTest(X, Y);
		if (Hit) return Hit;
	}
	if (SideRB)
	{
		SWidget* Hit = SideRB->HitTest(X, Y);
		if (Hit) return Hit;
	}

	// SideLT/SideRB 어디에도 없으면 스플리터 바 영역 → this
	return this;
}

bool SSplitter::OnMouseButtonDown(int32 Button, int32 X, int32 Y)
{
	if (Button != 0) return false; // 좌클릭만

	bDragging = true;
	FSlateApplication::Get().SetCapturedWidget(this);
	return true;
}

bool SSplitter::OnMouseMove(int32 X, int32 Y)
{
	if (!bDragging) return false;

	// 서브클래스(H/V)가 축에 맞는 비율을 계산합니다.
	const float NewRatio = ComputeNewRatio(X, Y);
	SetSplitRatio(NewRatio);
	UpdateChildRect();

	// 연결된 스플리터가 있으면 동일한 비율로 동기화합니다. (TopSplitterH ↔ BotSplitterH)
	if (LinkedSplitter)
	{
		LinkedSplitter->SetSplitRatio(NewRatio);
		LinkedSplitter->UpdateChildRect();
	}
	return true;
}

bool SSplitter::OnMouseButtonUp(int32 Button, int32 X, int32 Y)
{
	if (Button != 0 || !bDragging) return false;

	bDragging = false;
	FSlateApplication::Get().SetCapturedWidget(nullptr);
	return true;
}
