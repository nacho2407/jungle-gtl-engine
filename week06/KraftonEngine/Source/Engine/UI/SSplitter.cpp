#include "UI/SSplitter.h"

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

	float Half = SplitBarSize * 0.5f;
	float SplitX = ParentRect.X + ParentRect.Width * Ratio;

	// 분할 바 영역
	SplitBarRect = { SplitX - Half, ParentRect.Y, SplitBarSize, ParentRect.Height };

	// 왼쪽 자식
	if (SideLT)
	{
		FRect LeftRect = { ParentRect.X, ParentRect.Y, SplitX - Half - ParentRect.X, ParentRect.Height };
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
		float RightX = SplitX + Half;
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

	float Half = SplitBarSize * 0.5f;
	float SplitY = ParentRect.Y + ParentRect.Height * Ratio;

	// 분할 바 영역
	SplitBarRect = { ParentRect.X, SplitY - Half, ParentRect.Width, SplitBarSize };

	// 위쪽 자식
	if (SideLT)
	{
		FRect TopRect = { ParentRect.X, ParentRect.Y, ParentRect.Width, SplitY - Half - ParentRect.Y };
		SideLT->SetRect(TopRect);

		if (SSplitter* ChildSplitter = AsSplitter(SideLT))
		{
			ChildSplitter->ComputeLayout(TopRect);
		}
	}

	// 아래쪽 자식
	if (SideRB)
	{
		float BottomY = SplitY + Half;
		FRect BottomRect = { ParentRect.X, BottomY, ParentRect.Width, ParentRect.Y + ParentRect.Height - BottomY };
		SideRB->SetRect(BottomRect);

		if (SSplitter* ChildSplitter = AsSplitter(SideRB))
		{
			ChildSplitter->ComputeLayout(BottomRect);
		}
	}
}
