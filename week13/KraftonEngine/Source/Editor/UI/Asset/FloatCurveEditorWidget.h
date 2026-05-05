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
	int32 SelectedKeyIndex = -1;
	bool bDraggingSelectedKey = false;
	float ViewMinTime = 0.0f;
	float ViewMaxTime = 1.0f;
	float ViewMinValue = -1.0f;
	float ViewMaxValue = 1.0f;
};
