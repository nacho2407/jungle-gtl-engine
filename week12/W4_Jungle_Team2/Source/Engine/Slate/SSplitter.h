#pragma once
#include "SWindow.h"

/*
* SplitterH,SplitterV의 부모클래스
* - 루트는 SSplitterV
* - 그 안에 위쪽 SSplitterH, 아래쪽 SSplitterH
* - 총 4개 viewport 영역 생성
* - Editor.ini 에 저장할 값은 pixel 좌표가 아니라 ratio
* - drag 중에는 CapturedWidget = Splitter 설정
*/
class SSplitter : public SWindow
{
public:

	// Mouse Callbac
	bool OnMouseMove(int X, int Y) override;
	bool OnMouseButtonDown(int Button, int X, int Y) override;
	bool OnMouseButtonUp(int Button, int X, int Y) override;

	virtual void UpdateCildRect() { }

	/*
	* Getter Setter Section
	*/

	//SideLT (Left, Top)
	SWindow* GetSideLT() { return SideLT; }
	const SWindow* GetSideLT() const { return SideLT; }
	void SetSideLT(SWindow* InSideLT) { SideLT = InSideLT; }
	
	// SideRB (Right, Back)
	SWindow* GetSideRB() { return SideRB; }
	const SWindow* GetSideRB() const { return SideRB; }
	void SetSideRB(SWindow* InSideRB) { SideRB = InSideRB; }
	
	// SplitRatio
	void SetSplitRatio(float InRatio) { }
	float GetSplitRatio() const { return SplitRatio; }

private:
	SWindow* SideLT; // Left or Top
	SWindow* SideRB; // Right or Bottom

	float SplitRatio = 0.5f; // SideLT와 SideRB 분할 비율
	float BarThickness = 4.0f; // Splitter 바 두께
	bool bDragging = false;
};

