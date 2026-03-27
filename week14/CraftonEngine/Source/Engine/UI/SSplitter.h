#pragma once

#include "UI/SWindow.h"

// 스플리터 기반 — 두 개의 자식 창을 담아 영역을 분배하는 컨테이너
class SSplitter : public SWindow
{
public:
	SSplitter() = default;
	~SSplitter() override = default;

	void SetSideLT(SWindow* InSide) { SideLT = InSide; }
	void SetSideRB(SWindow* InSide) { SideRB = InSide; }
	SWindow* GetSideLT() const { return SideLT; }
	SWindow* GetSideRB() const { return SideRB; }

protected:
	SWindow* SideLT = nullptr;  // Left or Top
	SWindow* SideRB = nullptr;  // Right or Bottom
};

// 수평 분할 (좌/우)
class SSplitterH : public SSplitter
{
public:
	SSplitterH() = default;
	~SSplitterH() override = default;
};

// 수직 분할 (상/하)
class SSplitterV : public SSplitter
{
public:
	SSplitterV() = default;
	~SSplitterV() override = default;
};
