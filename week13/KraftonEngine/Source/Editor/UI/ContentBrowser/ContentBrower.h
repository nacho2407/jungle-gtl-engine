#pragma once
#include "imgui.h"
#include "Editor/UI/EditorWidget.h"
#include "Editor/UI/EditorDragSource.h"
#include "Platform/Paths.h"
#include <fstream>
#include <filesystem>


struct ContentBrowserContext final
{
	ImVec2 ContentSize;
};

class ContentBrowserElement
{
public:
	virtual void Render(ContentBrowserContext& Context) = 0;
};

class DefaultElement final : public ContentBrowserElement
{
public:
	void Render(ContentBrowserContext& Context) override;
};

struct FContentItem
{
	std::filesystem::path Path;
	std::wstring Name;
	bool bIsDirectory = false;
};

struct FDirNode
{
	FContentItem Self;
	TArray<FDirNode> Children;
};

class FEditorContextBrwoserWidget final : public FEditorWidget
{
public:
	void Initialize(UEditorEngine* InEditorEngine) override;
	void Render(float DeltaTime) override;
	void Refresh();

private:
	void RefreshContent();
	void DrawDirNode(FDirNode InNode);
	void DrawContents();

	TArray<FContentItem> ReadDirectory(std::wstring Path);
	FDirNode BuildDirectoryTree(const std::filesystem::path& DirPath);

private:
	std::wstring CurrentPath = FPaths::RootDir();

	FDirNode RootNode;
	TArray<FContentItem> CachedDirectories;
	TArray<std::unique_ptr<ContentBrowserElement>> CachedBrowserElements;
	ContentBrowserContext BrowserContext;
};