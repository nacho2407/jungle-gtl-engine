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

	FRect GetPaintClipRect() const override { return GetInteractiveRect(); }
	void OnPaint(FSlatePaintContext& Painter) override;
	bool OnMouseDown(int32 X, int32 Y) override;
	bool HitTest(FPoint Point) const override;
	FVector2 ComputeDesiredSize() const override;
	FVector2 ComputeMinSize() const override;

	void ConfigureForGlobalLayout();
	void ConfigureForViewport(FViewportId InViewportId);
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
	FViewportEntry* GetTargetEntry() const;
	bool IsTargetPIEViewport() const;

	void ApplyLayout(EViewportLayout NewLayout);
	void ApplyViewportType(EViewportType NewType);
	void ApplyRenderMode(ERenderMode NewMode);
	int32 EstimateTitleWidth() const;
	bool ShouldShowPIECaptureHint() const;
	void ApplyTitleButtonStyle();

private:
	FEditorEngine* Engine = nullptr;
	SButton TitleButton;
	SDropdown LayoutDropdown;
	SDropdown TypeDropdown;
	SDropdown ModeDropdown;
	FRect PIECaptureHintRect;
	bool bShowLayout = true;
	bool bShowViewportSettings = true;
	FViewportId TargetViewportId = INVALID_VIEWPORT_ID;

	uint32 TitleBackgroundColor = 0xFF1F1F1F;
	uint32 TitleBorderColor = 0xFF4E4E4E;
	uint32 TitleTextColor = 0xFFBDBDBD;
};

