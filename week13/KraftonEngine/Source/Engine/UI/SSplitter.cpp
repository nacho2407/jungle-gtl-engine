#include "UI/SSplitter.h"

#include <algorithm>
#include <cmath>

void SSplitter::SetRatio(float InRatio)
{
	Ratio = (std::max)(0.0f, (std::min)(InRatio, 1.0f));
	TargetRatio = Ratio;
	bIsAnimating = false;
}

void SSplitter::SetTargetRatio(float InRatio, bool bAnimate)
{
	TargetRatio = (std::max)(0.0f, (std::min)(InRatio, 1.0f));
	if (!bAnimate)
	{
		Ratio = TargetRatio;
		bIsAnimating = false;
		return;
	}

	bIsAnimating = std::fabs(Ratio - TargetRatio) >= 0.001f;
}

bool SSplitter::UpdateAnimation(float DeltaTime)
{
	if (!bIsAnimating)
	{
		return false;
	}

	const float EffectiveDeltaTime = DeltaTime > 0.0f ? DeltaTime : 1.0f / 60.0f;
	const float Alpha = (std::min)(EffectiveDeltaTime * 10.0f, 1.0f);
	Ratio = Ratio + (TargetRatio - Ratio) * Alpha;

	if (std::fabs(Ratio - TargetRatio) < 0.001f)
	{
		Ratio = TargetRatio;
		bIsAnimating = false;
	}

	return bIsAnimating;
}

void SSplitter::StopAnimation(bool bSnapToTarget)
{
	if (bSnapToTarget)
	{
		Ratio = TargetRatio;
	}
	else
	{
		TargetRatio = Ratio;
	}
	bIsAnimating = false;
}

SSplitter* SSplitter::AsSplitter(SWindow* InWindow)
{
	if (InWindow && InWindow->IsSplitter())
	{
		return static_cast<SSplitter*>(InWindow);
	}
	return nullptr;
}

void SSplitter::CollectSplitters(SSplitter* Node, TArray<SSplitter*>& OutSplitters)
{
	if (!Node) return;
	OutSplitters.push_back(Node);
	if (SSplitter* LT = AsSplitter(Node->GetSideLT()))
		CollectSplitters(LT, OutSplitters);
	if (SSplitter* RB = AsSplitter(Node->GetSideRB()))
		CollectSplitters(RB, OutSplitters);
}

void SSplitter::DestroyTree(SSplitter* Node)
{
	if (!Node) return;
	if (SSplitter* LT = AsSplitter(Node->GetSideLT()))
		DestroyTree(LT);
	if (SSplitter* RB = AsSplitter(Node->GetSideRB()))
		DestroyTree(RB);
	Node->SetSideLT(nullptr);
	Node->SetSideRB(nullptr);
	delete Node;
}

SSplitter* SSplitter::FindSplitterAtBar(SSplitter* Node, FPoint MousePos)
{
	if (!Node) return nullptr;
	if (Node->IsOverSplitBar(MousePos))
		return Node;
	if (SSplitter* Found = FindSplitterAtBar(AsSplitter(Node->GetSideLT()), MousePos))
		return Found;
	return FindSplitterAtBar(AsSplitter(Node->GetSideRB()), MousePos);
}

bool SSplitter::IsOverSplitBar(FPoint MousePos) const
{
	return MousePos.X >= SplitBarRect.X && MousePos.X < SplitBarRect.X + SplitBarRect.Width
		&& MousePos.Y >= SplitBarRect.Y && MousePos.Y < SplitBarRect.Y + SplitBarRect.Height;
}

// ─── SSplitterH: 좌/우 분할 ────────────────────────────────
void SSplitterH::ComputeLayout(const FRect& ParentRect)
{
	Rect = ParentRect;

	const float Half = SplitBarSize * 0.5f;
	const float SplitX = ParentRect.X + ParentRect.Width * Ratio;
	const float ParentRight = ParentRect.X + ParentRect.Width;
	const bool bCollapseLT = Ratio <= 0.001f;
	const bool bCollapseRB = Ratio >= 0.999f;

	// 분할 바 영역
	SplitBarRect = { SplitX - Half, ParentRect.Y, SplitBarSize, ParentRect.Height };

	// 왼쪽 자식
	if (SideLT)
	{
		const float LeftRight = bCollapseLT ? ParentRect.X : (bCollapseRB ? ParentRight : (std::max)(ParentRect.X, (std::min)(SplitX - Half, ParentRight)));
		FRect LeftRect = { ParentRect.X, ParentRect.Y, LeftRight - ParentRect.X, ParentRect.Height };
		SideLT->SetRect(LeftRect);

		// 자식이 SSplitter이면 재귀 ComputeLayout
		if (SSplitter* ChildSplitter = AsSplitter(SideLT))
		{
			ChildSplitter->ComputeLayout(LeftRect);
		}
	}

	// 오른쪽 자식
	if (SideRB)
	{
		const float RightX = bCollapseLT ? ParentRect.X : (bCollapseRB ? ParentRight : (std::max)(ParentRect.X, (std::min)(SplitX + Half, ParentRight)));
		FRect RightRect = { RightX, ParentRect.Y, ParentRect.X + ParentRect.Width - RightX, ParentRect.Height };
		SideRB->SetRect(RightRect);

		if (SSplitter* ChildSplitter = AsSplitter(SideRB))
		{
			ChildSplitter->ComputeLayout(RightRect);
		}
	}
}

// ─── SSplitterV: 상/하 분할 ────────────────────────────────
void SSplitterV::ComputeLayout(const FRect& ParentRect)
{
	Rect = ParentRect;

	const float Half = SplitBarSize * 0.5f;
	const float SplitY = ParentRect.Y + ParentRect.Height * Ratio;
	const float ParentBottom = ParentRect.Y + ParentRect.Height;
	const bool bCollapseLT = Ratio <= 0.001f;
	const bool bCollapseRB = Ratio >= 0.999f;

	// 분할 바 영역
	SplitBarRect = { ParentRect.X, SplitY - Half, ParentRect.Width, SplitBarSize };

	// 위쪽 자식
	if (SideLT)
	{
		const float TopBottom = bCollapseLT ? ParentRect.Y : (bCollapseRB ? ParentBottom : (std::max)(ParentRect.Y, (std::min)(SplitY - Half, ParentBottom)));
		FRect TopRect = { ParentRect.X, ParentRect.Y, ParentRect.Width, TopBottom - ParentRect.Y };
		SideLT->SetRect(TopRect);

		if (SSplitter* ChildSplitter = AsSplitter(SideLT))
		{
			ChildSplitter->ComputeLayout(TopRect);
		}
	}

	// 아래쪽 자식
	if (SideRB)
	{
		const float BottomY = bCollapseLT ? ParentRect.Y : (bCollapseRB ? ParentBottom : (std::max)(ParentRect.Y, (std::min)(SplitY + Half, ParentBottom)));
		FRect BottomRect = { ParentRect.X, BottomY, ParentRect.Width, ParentRect.Y + ParentRect.Height - BottomY };
		SideRB->SetRect(BottomRect);

		if (SSplitter* ChildSplitter = AsSplitter(SideRB))
		{
			ChildSplitter->ComputeLayout(BottomRect);
		}
	}
}
