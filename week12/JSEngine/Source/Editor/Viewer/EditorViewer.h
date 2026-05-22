#pragma once

#include "Core/CoreTypes.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/Viewport/SkeletalMeshViewportClient.h"
#include "Editor/UI/EditorTabManager.h"

class UEditorEngine;
class UWorld;
class FSelectionManager;
class FWindowsWindow;
class ASkeletalMeshActor;
class USkeletalMeshComponent;
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

	FSkeletalMeshViewportClient& GetClient() { return Client; }
	const FSkeletalMeshViewportClient& GetClient() const { return Client; }
	
	ASkeletalMeshActor* GetViewTarget() const { return ViewTarget;}
	void ClearViewTarget() { ViewTarget = nullptr; }
	
	const FString& GetFileName() const { return FileName; }
	void SetFileName(const FString& InFileName) { FileName = InFileName; }

protected:
	USkeletalMeshComponent* GetSkeletalMeshComponent() const;
	void ClearBaseSelection();

	virtual bool HandleViewportBonePick(float LocalX, float LocalY);

private:
	FSceneViewport Viewport;
	FSkeletalMeshViewportClient Client;
	ASkeletalMeshActor* ViewTarget = nullptr;
	FString FileName;
};
