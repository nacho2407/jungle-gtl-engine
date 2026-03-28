#pragma once
#include <filesystem>
#include <imgui.h>
#include <functional>
#include "Types/String.h"

struct ID3D11ShaderResourceView;

class CContentBrowserWindow
{
public:
	CContentBrowserWindow();

	void Render();

	void SetFolderIcon(ID3D11ShaderResourceView* FolderSRV);
	void SetFileIcon(ID3D11ShaderResourceView* FileSRV);

	std::function<void(const FString& FilePath)> OnFileDoubleClickCallback;
	std::function<void(const FString& DraggingFilePath, const FString& ReleaseDirectory)> OnFileDragEnd;

	bool IsHovered() const { return bIsHovered; }
	bool IsMouseOnDirectory() const { return bIsMouseOnDirectory; }
	bool IsMouseOnFile() const { return bIsMouseOnFile; }

private:
	std::filesystem::path RootPath;
	std::filesystem::path CurrentPath;
	std::filesystem::path SelectedPath;
	// Dragging 대상
	std::filesystem::path SelectedFilePath;

	std::filesystem::path DirectoryPathUnderMouse;
	std::filesystem::path FilePathUnderMouse;

	bool bFileOnDrag = false;
	bool bIsHovered = false;

	bool bIsMouseOnDirectory = false;
	bool bIsMouseOnFile = false;

	ImTextureID FolderIcon;  /* (ImTextureID)FolderSRV */
	ImTextureID FileIcon;

	void DrawFolderTree(const std::filesystem::path& Path);
	void DrawFileGrid();
};


