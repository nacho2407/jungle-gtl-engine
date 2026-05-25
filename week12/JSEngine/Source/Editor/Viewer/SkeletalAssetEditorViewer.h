#pragma once

#include "Editor/Viewer/EditorViewer.h"
#include "Editor/Viewport/Viewer/SkeletalMeshViewerViewportClient.h"

class ASkeletalMeshActor;
class USkeletalMeshComponent;

class FSkeletalAssetEditorViewer : public FEditorViewer
{
public:
	void Init(FWindowsWindow* InWindow, UEditorEngine* InEditor, UWorld* InWorld, FSelectionManager* InSelectionManager) override;
	void Shutdown() override;

	FSkeletalMeshViewerViewportClient& GetClient() override { return Client; }
	const FSkeletalMeshViewerViewportClient& GetClient() const override { return Client; }

	ASkeletalMeshActor* GetViewTarget() const { return ViewTarget; }
	void ClearViewTarget() { ViewTarget = nullptr; }

protected:
	USkeletalMeshComponent* GetSkeletalMeshComponent() const;
	virtual bool HandleViewportBonePick(float LocalX, float LocalY);

private:
	FSkeletalMeshViewerViewportClient Client;
	ASkeletalMeshActor* ViewTarget = nullptr;
};
