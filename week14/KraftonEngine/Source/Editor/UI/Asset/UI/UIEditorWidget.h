#pragma once

#include "Core/Types/CoreTypes.h"
#include "Editor/UI/Asset/UI/UIEditorDocument.h"

#include <filesystem>

class UUserWidget;

class FUIEditorWidget
{
public:
	void Open(const std::filesystem::path& InPath);
	void Close();
	void Render(float DeltaTime);

	bool IsOpen() const { return bOpen; }

private:
	void RenderToolbar();
	void RenderElementList();
	void RenderInspector();
	void RenderPreviewControls();
	void RenderStatusBar();

	void AddTextElement();
	bool Save();
	void RefreshPreview();
	void ShowPreview();
	void HidePreview();
	void MarkDirty();
	bool Validate(FString* OutError) const;
	FString GetDocumentPathString() const;

private:
	FUIEditorDocument Document;
	UUserWidget* PreviewWidget = nullptr;
	int32 SelectedElementIndex = -1;
	bool bOpen = false;
	bool bDirty = false;
	FString StatusText;
};
