#pragma once
#include "SWindow.h"

/*
* SplitterH,SplitterV의 부모클래스
* - Editor.ini 에 저장할 값은 pixel 좌표가 아니라 ratio
* - drag 중에는 CapturedWidget = Splitter 설정
*/
class SSplitter : public SWindow
{
public:

	// Mouse Callback
	bool OnMouseMove(int32 X, int32 Y) override;
	bool OnMouseButtonDown(int32 Button, int32 X, int32 Y) override;
	bool OnMouseButtonUp(int32 Button, int32 X, int32 Y) override;

	// SideLT / SideRB 를 먼저 검사하고, 그 사이 바 영역이면 this 반환
	SWidget* HitTest(int32 X, int32 Y) override;

	void UpdateChildRect() override { }

	/*
	 * 마우스 좌표로부터 새 SplitRatio 를 계산합니다.
	 * SSplitterH : X 축 기준, SSplitterV : Y 축 기준.
	 * [0.05, 0.95] 범위로 클램핑하여 완전히 닫히는 것을 방지합니다.
	 */
	virtual float ComputeNewRatio(int32 X, int32 Y) const = 0;

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
	void SetSplitRatio(float InRatio) { SplitRatio = InRatio; }
	float GetSplitRatio() const { return SplitRatio; }

	bool IsDragging() const { return bDragging; }
	float GetBarThickness() const { return BarThickness; }

	// 바 영역의 FRect를 반환합니다. (ImGui 등 외부 렌더러에서 그리기 용도)
	virtual FRect GetBarRect() const = 0;

	// 드래그 시 함께 움직일 스플리터 (TopSplitterH ↔ BotSplitterH 동기화용)
	SSplitter* GetLinkedSplitter() const { return LinkedSplitter; }
	void SetLinkedSplitter(SSplitter* InLinked) { LinkedSplitter = InLinked; }

private:
	SWindow* SideLT = nullptr; // Left or Top
	SWindow* SideRB = nullptr; // Right or Bottom

	float SplitRatio    = 0.5f; // SideLT와 SideRB 분할 비율
	float BarThickness  = 6.0f; // Splitter 바 두께 (HitTest 여백 + 시각적 두께)
	bool  bDragging     = false;
	SSplitter* LinkedSplitter = nullptr;
};
