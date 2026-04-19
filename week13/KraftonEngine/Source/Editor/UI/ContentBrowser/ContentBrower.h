#pragma once
#include "imgui.h"
#include "Editor/UI/EditorWidget.h"
#include "Editor/UI/EditorDragSource.h"

struct ContentBrowserContext final
{
	ImVec2 ContentSize;
};

class ContentBrowserElement
{
public:
	virtual void Render(ContentBrowserContext& Context) = 0;
};

class ContextBrwoser final : public FEditorWidget
{
public:
	void Render(float DeltaTime) override;
	void Refresh();

private:

	TArray<ContentBrowserElement*> BrowserElements;
	ContentBrowserContext BrowserContext;
};