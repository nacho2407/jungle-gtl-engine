#pragma once
#include "Editor/UI/Asset/AssetEditorWidget.h"

class FCameraShakeEditorWidget : public FAssetEditorWidget
{
public:
	FCameraShakeEditorWidget() = default;

	virtual bool CanEdit(UObject* Object) const override;

	virtual void Open(UObject* Object) override;

	virtual void Render(const FEditorPanelContext& Context) override;

private:
	bool bPreviewPlaying = false;
	bool bPreviewLoop = true;
	float PreviewTime = 0.0f;
};
