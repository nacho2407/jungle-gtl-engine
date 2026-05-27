#pragma once
#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Editor/UI/Asset/Curve/InlineFloatCurveEditor.h"

class FFloatCurveEditorWidget : public FAssetEditorWidget
{
public:
	FFloatCurveEditorWidget() = default;

	virtual bool CanEdit(UObject* Object) const override;

	virtual void Open(UObject* Object) override;

	virtual void Render(float DeltaTime) override;

private:
	void FitViewToCurve();

	FInlineFloatCurveEditor InlineEditor;
};
