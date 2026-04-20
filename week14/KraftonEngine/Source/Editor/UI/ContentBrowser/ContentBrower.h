#pragma once
#include "imgui.h"
#include "Editor/UI/EditorWidget.h"
#include "Editor/UI/EditorDragSource.h"
#include "Platform/Paths.h"
#include <fstream>
#include <filesystem>
#include <d3d11.h>

struct FContentItem
{
	std::filesystem::path Path;
	std::wstring Name;
	bool bIsDirectory = false;
};

class ContentBrowserElement;
struct ContentBrowserContext final
{
	ImVec2 ContentSize;
	ContentBrowserElement* SelectedElement;
};

class ContentBrowserElement
{
public:
	virtual void Render(ContentBrowserContext& Context) = 0;
	void SetIcon(ID3D11ShaderResourceView* InIcon) { Icon = InIcon; }
	void SetContent(FContentItem InContent) { ContentItem = InContent; }

protected:
	FString EllipsisText(const FString& text, float maxWidth);

protected:
	bool bIsSelected = false;
	ID3D11ShaderResourceView* Icon;
	FContentItem ContentItem;
};

class DefaultElement final : public ContentBrowserElement
{
public:
	void Render(ContentBrowserContext& Context) override;
};

struct FDirNode
{
	FContentItem Self;
	TArray<FDirNode> Children;
};

class FEditorContextBrwoserWidget final : public FEditorWidget
{
public:
	void Initialize(UEditorEngine* InEditor, ID3D11Device* InDevice);
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

	ID3D11ShaderResourceView* DefaultIcon = nullptr;
	ID3D11ShaderResourceView* FolderIcon = nullptr;
};