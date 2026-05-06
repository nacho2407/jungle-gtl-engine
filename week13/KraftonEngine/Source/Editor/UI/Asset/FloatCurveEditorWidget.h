#pragma once
#include "AssetEditorWidget.h"

class FFloatCurveEditorWidget : public FAssetEditorWidget
{
public:
	FFloatCurveEditorWidget() = default;

	virtual bool CanEdit(UObject* Object) const override;

	virtual void Open(UObject* Object) override;

	virtual void Render(float DeltaTime) override;

private:
	void FitViewToCurve();

	int32 SelectedKeyIndex = -1;
	bool bDraggingSelectedKey = false;
	bool bPanningView = false;
	bool bSuppressNextCanvasContextMenu = false;
	float PendingContextTime = 0.0f;
	float PendingContextValue = 0.0f;
	float ViewMinTime = 0.0f;
	float ViewMaxTime = 1.0f;
	float ViewMinValue = -1.0f;
	float ViewMaxValue = 1.0f;
};
