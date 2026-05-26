#pragma once

#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"
#include "GameFramework/AActor.h"
#include "Editor/Viewport/EditorPreviewViewportClient.h"
#include "Editor/Viewport/Asset/StaticMeshEditorViewportClient.h"
#include "Editor/Viewport/Asset/MeshEditorViewportClient.h"
#include "Object/FName.h"

#include <d3d11.h>

enum class EMeshThumbnailType
{
	StaticMesh,
	SkeletalMesh,
};

struct FMeshThumbnailEntry
{
	FString AssetPath;
	EMeshThumbnailType Type;
	
	FName PreviewWorldHandle;
	UWorld* PreviewWorld = nullptr;
	AActor* PreviewActor = nullptr;

	std::unique_ptr<FStaticMeshEditorViewportClient> StaticViewport;
	std::unique_ptr<FMeshEditorViewportClient> SkeletalViewport;

	bool bRequested = false;
	bool bReady = false;
	int32 WarmupFrames = 2;
};

class FEditorMeshThumbnailManager : public TSingleton<FEditorMeshThumbnailManager>
{
public:
	void Initialize(ID3D11Device* Device);
	void Shutdown();

	ID3D11ShaderResourceView* GetOrRequestThumbnail(const FString& AssetPath, EMeshThumbnailType Type);
	void Tick(float DeltaTime);

	void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const;
	void ClearThumbnails();

private:
	FMeshThumbnailEntry* FindOrCreateEntry(const FString& AssetPath, EMeshThumbnailType Type);
	void BuildPreviewScene(FMeshThumbnailEntry& Entry);

private:
	ID3D11Device* Device = nullptr;

	TMap<FString, std::unique_ptr<FMeshThumbnailEntry>> Entries;
	TArray<FString> PendingRequests;

	static FString MakeKey(const FString& AssetPath, EMeshThumbnailType Type);
};
