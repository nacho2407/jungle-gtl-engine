#pragma once

#include "Viewport/ViewportTypes.h"
#include "SViewport.h"
#include "SplitterH.h"
#include "SplitterV.h"
#include <memory>

class FSlateApplication
{
	FViewportId HoveredViewportId       = INVALID_VIEWPORT_ID;
	FViewportId FocusedViewportId       = INVALID_VIEWPORT_ID;
	FViewportId MouseCapturedViewportId = INVALID_VIEWPORT_ID;
	SSplitter*  DraggingSplitter        = nullptr;

	std::unique_ptr<SSplitterH> SplitterH;
	std::unique_ptr<SSplitterV> SplitterVL;
	std::unique_ptr<SSplitterV> SplitterVR;
	std::unique_ptr<SViewport>  Viewports[4];

public:
	void Initialize(const FRect& Area,
	                FViewport* VP0, FViewport* VP1,
	                FViewport* VP2, FViewport* VP3,
	                float RatioH, float RatioVL, float RatioVR);
	
	void SetViewportAreaRect(const FRect& Area);

	void PerformLayout();

	FViewportId GetHoveredViewportId()       const { return HoveredViewportId; }
	FViewportId GetFocusedViewportId()       const { return FocusedViewportId; }
	FViewportId GetMouseCapturedViewportId() const { return MouseCapturedViewportId; }
	bool IsPointerOverViewport(FViewportId Id) const;
	bool IsDraggingSplitter()                const { return DraggingSplitter != nullptr; }

	SSplitterH* GetSplitterH()               const { return SplitterH.get(); }
	SSplitterV* GetSplitterVL()              const { return SplitterVL.get(); }
	SSplitterV* GetSplitterVR()              const { return SplitterVR.get(); }
	SViewport*  GetViewportWidget(int32 Idx) const;

	void ProcessMouseDown(int32 X, int32 Y);
	void ProcessMouseMove(int32 X, int32 Y);
	void ProcessMouseUp(int32 X, int32 Y);
};
