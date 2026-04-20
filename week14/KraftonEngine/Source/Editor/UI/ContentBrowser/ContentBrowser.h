#pragma once
#include "Editor/UI/EditorWidget.h"
#include "ContentItem.h"
#include <d3d11.h>
#include <memory>
#include "ContentBrowserContext.h"
#include "ContentBrowserElement.h"

class FEditorContentBrowserWidget final : public FEditorWidget
{
	struct FDirNode
	{
		FContentItem Self;
		TArray<FDirNode> Children;
	};

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
	ContentBrowserContext BrowserContext;

	FDirNode RootNode;
	TArray<std::unique_ptr<ContentBrowserElement>> CachedBrowserElements;

	ID3D11ShaderResourceView* DefaultIcon = nullptr;
	ID3D11ShaderResourceView* FolderIcon = nullptr;
};
