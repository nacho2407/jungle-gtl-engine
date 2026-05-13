#pragma once

#include "Editor/UI/EditorWidget.h"
#include "UI/RuntimeUITypes.h"

#include <functional>

class FEditorRuntimeUIPreviewWidget : public FEditorWidget
{
public:
	void Initialize(UEditorEngine* InEditorEngine) override;
	void Render(float DeltaTime) override;
	void SetRmlRenderQueue(std::function<void(const FRuntimeUIRenderContext&)> InQueueCallback);

private:
	void DrawToolbar();
	void DrawPreviewSurface(float DeltaTime);
	void DrawDocumentInfo() const;
	void DrawActionEvents();
	void DrawAuthoringGuidance() const;
	bool LoadPreviewDocument();
	void RefreshPreviewDocument();
	bool OpenRmlFileDialog(FString& OutPath) const;
	bool SetPreviewDocumentPath(const FString& Path);
	bool AcceptRmlDragDropTarget();

private:
	std::function<void(const FRuntimeUIRenderContext&)> QueueRmlRenderContext;
	TArray<FString> PreviewActionEvents;
	char PreviewScreenIdBuffer[64] = "__RuntimeUIPreview";
	char PreviewDocumentPathBuffer[260] = "Asset/UI/Game/Title.rml";
	int32 ResolutionPresetIndex = 0;
	int32 CustomWidth = 1920;
	int32 CustomHeight = 1080;
	float PreviewZoom = 1.0f;
	bool bEnableInteraction = true;
	bool bShowGuidance = true;
	bool bPreviewDocumentLoaded = false;
};
