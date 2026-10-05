#include "SkeletalAssetEditorViewer.h"

#include "Component/SkeletalMeshComponent.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"

void FSkeletalAssetEditorViewer::Init(
	FWindowsWindow* InWindow,
	UEditorEngine* InEditor,
	UWorld* InWorld,
	FSelectionManager* InSelectionManager)
{
	FEditorViewer::Init(InWindow, InEditor, InWorld, InSelectionManager);

	Client.SetBonePickHandler([this](float LocalX, float LocalY)
	{
		return HandleViewportBonePick(LocalX, LocalY);
	});

	ViewTarget = InWorld->SpawnActor<ASkeletalMeshActor>();
	ViewTarget->InitDefaultComponents();
	ViewTarget->SetFName(FName("ViewerActor"));
	ViewTarget->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

	InWorld->SyncSpatialIndex();
}

void FSkeletalAssetEditorViewer::Shutdown()
{
	ViewTarget = nullptr;
	Client.SetBonePickHandler(nullptr);
	FEditorViewer::Shutdown();
}

USkeletalMeshComponent* FSkeletalAssetEditorViewer::GetSkeletalMeshComponent() const
{
	return ViewTarget ? ViewTarget->GetSkeletalMeshComponent() : nullptr;
}

bool FSkeletalAssetEditorViewer::HandleViewportBonePick(float LocalX, float LocalY)
{
	(void)LocalX;
	(void)LocalY;
	return false;
}
