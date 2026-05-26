#pragma once

#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"
#include "Editor/Viewport/EditorPreviewViewportClient.h"
#include "Editor/Viewport/Asset/StaticMeshEditorViewportClient.h"
#include "Object/FName.h"

#include <d3d11.h>
#include <memory>

class AActor;
class UStaticMeshComponent;
class UWorld;

struct FMaterialThumbnailEntry
{
	FString AssetPath;
	FName PreviewWorldHandle;

	UWorld* PreviewWorld = nullptr;
	AActor* PreviewActor = nullptr;
	UStaticMeshComponent* PreviewMeshComponent = nullptr;

	std::unique_ptr<FStaticMeshEditorViewportClient> Viewport;

	bool bRequested = false;
	bool bReady = false;
	int32 WarmupFrames = 2;
};

class FEditorMaterialThumbnailManager : public TSingleton<FEditorMaterialThumbnailManager>
{
public:
	void Initialize(ID3D11Device* InDevice);
	void Shutdown();

	ID3D11ShaderResourceView* GetOrRequestThumbnail(const FString& AssetPath);
	void Tick(float DeltaTime);

	void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const;
	void ClearThumbnails();

private:
	FMaterialThumbnailEntry* FindOrCreateEntry(const FString& AssetPath);
	void BuildPreviewScene(FMaterialThumbnailEntry& Entry);

private:
	ID3D11Device* Device = nullptr;

	TMap<FString, std::unique_ptr<FMaterialThumbnailEntry>> Entries;
	TArray<FString> PendingRequests;
};
