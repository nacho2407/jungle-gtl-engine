#pragma once
#include "imgui.h" 
#include "Platform/Paths.h"
#include "Asset/AssetRegistry.h"
#include "Editor/UI/Dialog/FbxImportOptionsDialog.h"
#include "Editor/UI/Dialog/PhysicsAssetCreationDialog.h"
#include <memory>

class ContentBrowserElement;
class UEditorEngine;

/**
 * @brief Content Browser 에셋 표시 방식
 */
enum class EContentBrowserViewMode
{
	Grid,
	List
};

struct ContentBrowserContext final
{
	std::wstring CurrentPath = FPaths::RootDir();
	std::wstring PendingRevealPath;
	ImVec2 ContentSize = ImVec2(50.0f, 50.0f);
	EContentBrowserViewMode ViewMode = EContentBrowserViewMode::Grid;
	std::shared_ptr<ContentBrowserElement> SelectedElement;

	UEditorEngine* EditorEngine;

	bool bPendingContentRefresh = false;

	// SelectedElement 에 대한 rename popup 요청 — F2 키 또는 우클릭 메뉴의 Rename 이 true 로 set.
	// ContentBrowser::Render 가 다음 프레임 popup 열고 false 로 reset.
	bool bRenameRequested = false;

	bool bDeleteRequested = false;

	// Shared FBX import options modal state. MeshElement fills this on double-click and
	// ContentBrowser renders the common modal once per frame.
	FFbxSceneImportDialogState FbxImportDialog;
	
	FPhysicsAssetCreationDialogState PhysicsAssetDialog;
};
