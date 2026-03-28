#include "SSplitter.h"

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

bool SSplitter::OnMouseMove(int32 X, int32 Y)
{
	return false;
}

bool SSplitter::OnMouseButtonDown(int32 Button, int32 X, int32 Y)
{
	return false;
}

bool SSplitter::OnMouseButtonUp(int32 Button, int32 X, int32 Y)
{
	return false;
}
