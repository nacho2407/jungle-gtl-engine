#pragma once

#include "Editor/Viewport/EditorViewportClient.h"
#include "Math/Color.h"

// Asset viewer-local display options. These do not inherit Level Editor show flags.
class FViewerViewportClient : public FEditorViewportClient
{
public:
	void SetRealtime(bool bInRealtime);
	bool IsRealtime() const { return bRealtime; }

	void SetShowGrid(bool bInShowGrid) { bShowGrid = bInShowGrid; }
	bool IsShowGrid() const { return bShowGrid; }

	void SetShowAxis(bool bInShowAxis) { bShowAxis = bInShowAxis; }
	bool IsShowAxis() const { return bShowAxis; }

	void SetShowBounds(bool bInShowBounds) { bShowBounds = bInShowBounds; }
	bool IsShowBounds() const { return bShowBounds; }

	void SetShowGizmo(bool bInShowGizmo) { bShowGizmo = bInShowGizmo; }
	bool IsShowGizmo() const { return bShowGizmo; }

	void SetBackgroundColor(const FColor& InColor) { BackgroundColor = InColor; }
	const FColor& GetBackgroundColor() const { return BackgroundColor; }

	void SetViewMode(EViewMode InViewMode);
	EViewMode GetViewMode() const;

	virtual void BuildViewerShowFlags(FShowFlags& OutShowFlags) const;

private:
	bool bRealtime = true;
	bool bShowGrid = true;
	bool bShowAxis = true;
	bool bShowBounds = false;
	bool bShowGizmo = true;
	FColor BackgroundColor = FColor(0.025f, 0.025f, 0.03f, 1.0f);
};
