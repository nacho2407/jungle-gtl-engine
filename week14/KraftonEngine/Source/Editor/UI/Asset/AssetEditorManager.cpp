#include "AssetEditorManager.h"

#include "AssetEditorWidget.h"
#include "Viewport/EditorPreviewViewportClient.h"

FAssetEditorManager::~FAssetEditorManager() = default;

void FAssetEditorManager::Tick(float DeltaTime)
{
	for (const auto& Editor : OpenEditors)
	{
		if (Editor->IsOpen())
		{
			Editor->Tick(DeltaTime);
		}
	}

	RemoveClosedEditors();
}

void FAssetEditorManager::Render(float DeltaTime)
{
	for (const auto& Editor : OpenEditors)
	{
		Editor->Render(DeltaTime);
	}
}

bool FAssetEditorManager::OpenEditorForObject(UObject* Object)
{
	for (const auto& Editor : OpenEditors)
	{
		if (Editor->CanEdit(Object) && !Editor->AllowsMultipleInstances())
		{
			Editor->Open(Object);
			return true;
		}
	}

	for (const auto& Factory : EditorFactories)
	{
		auto Editor = Factory();
		if (!Editor || !Editor->CanEdit(Object)) continue;

		Editor->Open(Object);
		OpenEditors.push_back(std::move(Editor));
		return true;
	}

	return false;
}

void FAssetEditorManager::CollectPreviewViewportClients(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	for (const auto& Editor : OpenEditors)
	{
		Editor->CollectPreviewViewports(OutClients);
	}
}

bool FAssetEditorManager::IsMouseOverAnyEditorViewport() const
{
	TArray<IEditorPreviewViewportClient*> PreviewViewportClients;
	CollectPreviewViewportClients(PreviewViewportClients);

	for (IEditorPreviewViewportClient* Client : PreviewViewportClients)
	{
		if (Client && Client->IsMouseOverViewport())
		{
			return true;
		}
	}

	return false;
}

void FAssetEditorManager::RemoveClosedEditors()
{
	OpenEditors.erase(std::remove_if(OpenEditors.begin(), OpenEditors.end(),
		[](const std::unique_ptr<FAssetEditorWidget>& Editor)
		{
			return !Editor || !Editor->IsOpen();
		}),
	OpenEditors.end());
}
