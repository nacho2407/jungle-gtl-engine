#include "AssetEditorManager.h"

#include "AssetEditorWidget.h"
#include "Viewport/EditorPreviewViewportClient.h"

FAssetEditorManager::~FAssetEditorManager() = default;

void FAssetEditorManager::Tick(float DeltaTime)
{
	for (const auto& Editor : Editors)
	{
		if (Editor->IsOpen())
		{
			Editor->Tick(DeltaTime);
		}
	}
}

void FAssetEditorManager::Render(float DeltaTime)
{
	for (const auto& Editor : Editors)
	{
		Editor->Render(DeltaTime);
	}
}

bool FAssetEditorManager::OpenEditorForObject(UObject* Object)
{
	for (const auto& Editor : Editors)
	{
		if (Editor->CanEdit(Object))
		{
			Editor->Open(Object);
			return true;
		}
	}
	return false;
}

void FAssetEditorManager::CollectPreviewViewportClients(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	for (const auto& Editor : Editors)
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
