#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"

struct ImVec2;
class UParticleSystem;

class FParticleSystemEditorWidget : public FAssetEditorWidget
{
public:
	FParticleSystemEditorWidget() = default;

	bool CanEdit(UObject* Object) const override;
	void Open(UObject* Object) override;
	void Close() override;
	void Tick(float DeltaTime) override;
	void Render(float DeltaTime) override;

private:
	UParticleSystem* GetParticleSystem() const;
	void ResetEditorState();
	void RenderToolbar();
	void RenderViewportPanel(const ImVec2& Size) const;
	void RenderDetailsPanel(const ImVec2& Size) const;
	void RenderEmittersPanel(const ImVec2& Size);
	void RenderCurveEditorPanel(const ImVec2& Size) const;

private:
	int32 SelectedEmitterIndex = -1;
	int32 SelectedLODIndex = 0;
	int32 SelectedModuleIndex = -1;
	bool bPreviewPlaying = true;
	float PreviewTime = 0.0f;
};
