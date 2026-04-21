#pragma once
#include "imgui.h" 
#include "Platform/Paths.h"
#include <memory>

class ContentBrowserElement;
class UEditorEngine;

struct ContentBrowserContext final
{
	std::wstring CurrentPath = FPaths::RootDir();
	ImVec2 ContentSize = ImVec2(50.0f, 50.0f);
	std::shared_ptr<ContentBrowserElement> SelectedElement;

	UEditorEngine* EditorEngine;

	bool bIsNeedRefresh = false;
};
