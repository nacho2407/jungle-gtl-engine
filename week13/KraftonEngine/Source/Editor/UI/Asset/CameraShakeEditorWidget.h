#pragma once
#include "AssetEditorWidget.h"

class FCameraShakeEditorWidget : public FAssetEditorWidget
{
public:
	FCameraShakeEditorWidget() = default;

	virtual bool CanEdit(UObject* Object) const override;

	virtual void Open(UObject* Object) override;

	virtual void Render(float DeltaTime) override;

private:
	bool bPreviewPlaying = false;
	bool bPreviewLoop = true;
	float PreviewTime = 0.0f;
};
