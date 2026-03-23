#pragma once
#include "Panel.h"
class FStatePanel : public IPanel
{
public:
	// PanelManager와 Window 메뉴에서 이 패널을 식별할 고정 ID입니다.
	const wchar_t* GetPanelID() const override;
	// 탭 제목과 Window 메뉴에 표시할 이름입니다.
	const wchar_t* GetDisplayName() const override;
	// 에디터 시작 시 바로 보이도록 기본 열림 상태로 둡니다.
	bool ShouldOpenByDefault() const override { return true; }
	// 현재 프레임 상태 통계를 ImGui 창으로 그립니다.
	void Draw() override;
};

