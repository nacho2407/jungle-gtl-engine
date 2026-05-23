#pragma once

#include "Core/CoreTypes.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/UI/EditorTabManager.h"

class UEditorEngine;
class UWorld;
class FSelectionManager;
class FWindowsWindow;
struct ID3D11ShaderResourceView;

class FEditorViewer
{
public:
	virtual ~FEditorViewer() = default;

	// Lifecycle
	virtual void Init(FWindowsWindow* InWindow, UEditorEngine* InEditor, UWorld* InWorld, FSelectionManager* InSelectionManager);
	virtual void Shutdown();
	virtual void Tick(float DeltaTime);

	// Base Interface
	virtual bool ChangeTarget(const FString& InFileName) = 0;
	virtual EEditorTabKind GetTabKind() const = 0;
	virtual const char* GetViewerLabel() const = 0;

	// Getter & Setter
	void SetRect(const FViewportRect& InRect);
	ID3D11ShaderResourceView* GetSRV() const { return Viewport.GetOutSRV(); }
	
	FSceneViewport& GetViewport() { return Viewport; }
	const FSceneViewport& GetViewport() const { return Viewport; }

	virtual FEditorViewportClient& GetClient() = 0;
	virtual const FEditorViewportClient& GetClient() const = 0;
	
	const FString& GetFileName() const { return FileName; }
	void SetFileName(const FString& InFileName) { FileName = InFileName; }

protected:
	void ClearBaseSelection();

private:
	FSceneViewport Viewport;
	FString FileName;
};
