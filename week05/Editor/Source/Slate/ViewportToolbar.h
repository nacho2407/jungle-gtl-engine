#pragma once

#include "Widget/Widget.h"
#include "Widget/Button.h"
#include "Widget/Dropdown.h"

class FEditorEngine;
struct FViewportEntry;

class SViewportToolbarWidget : public SWidget
{
public:
	explicit SViewportToolbarWidget(FEditorEngine* InEngine);

	void OnPaint(SWidget& Painter) override;
	bool OnMouseDown(int32 X, int32 Y) override;
	bool HitTest(FPoint Point) const override;
	void SetHeaderRect(const FRect& InRect);
	FRect GetInteractiveRect() const;
	bool HasOpenDropdown() const;

private:
	enum class EDropdownId
	{
		Layout,
		Type,
		Mode
	};

	void SyncSelectionState();
	void UpdateGeometry();

	bool HandleDropdownMouse(SDropdown& Dropdown, EDropdownId DropdownId, int32 X, int32 Y);
	void CloseAllDropdowns();
	void CloseOtherDropdowns(EDropdownId KeepOpen);

	EViewportLayout GetCurrentLayout() const;
	FViewportEntry* GetFocusedEntry() const;

	void ApplyLayout(EViewportLayout NewLayout);
	void ApplyViewportType(EViewportType NewType);
	void ApplyRenderMode(ERenderMode NewMode);

	static bool ContainsPoint(const FRect& InRect, FPoint Point);
	static FRect UnionRects(const FRect& A, const FRect& B);
	FRect GetExpandedInteractiveRect() const;

private:
	FEditorEngine* Engine = nullptr;
	SButton TitleButton;
	SDropdown LayoutDropdown;
	SDropdown TypeDropdown;
	SDropdown ModeDropdown;
};
