#pragma once
#include "SWindow.h"

class SSplitter : public SWindow
{
public:

	SWindow* GetSideLT() { return SideLT; }
	const SWindow* GetSideLT() const { return SideLT; }

	SWindow* GetSideRB() { return SideRB; }
	const SWindow* GetSideRB() const { return SideRB; }

	void SetSideLT(SWindow* InSideLT) { SideLT = InSideLT; }
	void SetSideRB(SWindow* InSideRB) { SideRB = InSideRB; }

private:
	SWindow* SideLT; // Left or Top
	SWindow* SideRB; // Right or Bottom
};

