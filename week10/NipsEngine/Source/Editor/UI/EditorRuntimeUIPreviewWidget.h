#pragma once

#include "Editor/UI/EditorWidget.h"
#include "UI/Backends/ImGuiRuntimeUIBackend.h"
#include "UI/RuntimeUISystem.h"

class FEditorRuntimeUIPreviewWidget : public FEditorWidget
{
public:
	void Initialize(UEditorEngine* InEditorEngine) override;
	void Render(float DeltaTime) override;

private:
	enum class EPreviewSource : int32
	{
		SampleGameJam,
		LiveRuntime
	};

	void RebuildPreviewUI();
	void DrawToolbar();
	void DrawPreviewSurface(float DeltaTime);
	void DrawScreenSelector(FRuntimeUISystem& UI, bool bCanMutate);
	void DrawWidgetSummary(const FRuntimeUISystem& UI) const;
	void DrawActionEvents();
	void DrawAuthoringGuidance() const;
	void HandlePreviewInput(FRuntimeUISystem& UI, const FRuntimeUIRenderContext& Context, bool bCanMutate);
	FRuntimeUIResolvedImage ResolveRuntimeUIImage(const FString& ImagePath) const;

	FRuntimeUISystem PreviewUI;
	FImGuiRuntimeUIBackend PreviewBackend;
	TArray<FString> PreviewActionEvents;
	EPreviewSource PreviewSource = EPreviewSource::SampleGameJam;

	int32 ResolutionPresetIndex = 0;
	int32 CustomWidth = 1920;
	int32 CustomHeight = 1080;
	float PreviewZoom = 1.0f;
	bool bEnableInteraction = true;
	bool bShowGuidance = true;
	bool bNeedsRebuild = true;
};
