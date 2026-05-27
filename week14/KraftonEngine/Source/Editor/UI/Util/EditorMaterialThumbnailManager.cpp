#include "Editor/UI/Util/EditorMaterialThumbnailManager.h"

#include "Component/Light/DirectionalLightComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "GameFramework/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Runtime/Engine.h"
#include "Viewport/Viewport.h"

void FEditorMaterialThumbnailManager::Initialize(ID3D11Device* InDevice)
{
	Device = InDevice;
}

void FEditorMaterialThumbnailManager::Shutdown()
{
	ClearThumbnails();
	Device = nullptr;
}

void FEditorMaterialThumbnailManager::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	for (const auto& Pair : Entries)
	{
		const FMaterialThumbnailEntry& Entry = *Pair.second;
		if (Entry.bReady)
		{
			continue;
		}

		if (Entry.Viewport && Entry.Viewport->IsRenderable())
		{
			OutClients.push_back(Entry.Viewport.get());
		}
	}
}

void FEditorMaterialThumbnailManager::ClearThumbnails()
{
	for (auto& Pair : Entries)
	{
		FMaterialThumbnailEntry& Entry = *Pair.second;

		if (Entry.Viewport)
		{
			Entry.Viewport->Release();
		}

		if (Entry.PreviewWorld && Entry.PreviewWorldHandle.IsValid())
		{
			GEngine->DestroyWorldContext(Entry.PreviewWorldHandle);
		}
	}

	Entries.clear();
	PendingRequests.clear();
}

ID3D11ShaderResourceView* FEditorMaterialThumbnailManager::GetOrRequestThumbnail(const FString& AssetPath)
{
	FMaterialThumbnailEntry* Entry = FindOrCreateEntry(AssetPath);
	if (!Entry)
	{
		return nullptr;
	}

	if (!Entry->bRequested)
	{
		Entry->bRequested = true;
		PendingRequests.push_back(AssetPath);
	}

	FViewport* Viewport = Entry->Viewport ? Entry->Viewport->GetViewport() : nullptr;
	return Entry->bReady && Viewport ? Viewport->GetSRV() : nullptr;
}

void FEditorMaterialThumbnailManager::Tick(float DeltaTime)
{
	(void)DeltaTime;

	constexpr int32 MaxBuildPerFrame = 1;

	int32 BuiltCount = 0;
	while (!PendingRequests.empty() && BuiltCount < MaxBuildPerFrame)
	{
		FString Key = PendingRequests.front();
		PendingRequests.erase(PendingRequests.begin());

		auto It = Entries.find(Key);
		if (It == Entries.end())
		{
			continue;
		}

		FMaterialThumbnailEntry& Entry = *It->second;
		if (!Entry.PreviewWorld)
		{
			BuildPreviewScene(Entry);
			++BuiltCount;
		}
	}

	for (auto& Pair : Entries)
	{
		FMaterialThumbnailEntry& Entry = *Pair.second;
		if (!Entry.PreviewWorld || Entry.bReady)
		{
			continue;
		}

		if (Entry.WarmupFrames > 0)
		{
			--Entry.WarmupFrames;
		}
		else
		{
			Entry.bReady = true;
		}
	}
}

FMaterialThumbnailEntry* FEditorMaterialThumbnailManager::FindOrCreateEntry(const FString& AssetPath)
{
	if (auto It = Entries.find(AssetPath); It != Entries.end())
	{
		return It->second.get();
	}

	auto NewEntry = std::make_unique<FMaterialThumbnailEntry>();
	NewEntry->AssetPath = AssetPath;
	NewEntry->PreviewWorldHandle = FName("MaterialThumbnail_" + std::to_string(Entries.size()));

	FMaterialThumbnailEntry* Raw = NewEntry.get();
	Entries[AssetPath] = std::move(NewEntry);
	return Raw;
}

void FEditorMaterialThumbnailManager::BuildPreviewScene(FMaterialThumbnailEntry& Entry)
{
	if (!Device || !GEngine)
	{
		return;
	}

	UMaterialInterface* Material = FMaterialManager::Get().GetOrCreateMaterialInterface(Entry.AssetPath);
	if (!Material)
	{
		return;
	}

	UStaticMesh* SphereMesh = FMeshManager::LoadStaticMesh("Content/Data/BasicShape/Sphere.OBJ", Device);
	if (!SphereMesh)
	{
		return;
	}

	FWorldContext& WorldContext =
		GEngine->CreateWorldContext(EWorldType::EditorPreview, Entry.PreviewWorldHandle);

	Entry.PreviewWorld = WorldContext.World;
	if (!Entry.PreviewWorld)
	{
		return;
	}

	Entry.PreviewWorld->SetWorldType(EWorldType::EditorPreview);
	Entry.PreviewWorld->InitWorld();

	Entry.PreviewActor = Entry.PreviewWorld->SpawnActor<AActor>();
	if (!Entry.PreviewActor)
	{
		return;
	}

	UStaticMeshComponent* Comp = Entry.PreviewActor->AddComponent<UStaticMeshComponent>();
	Comp->SetStaticMesh(SphereMesh);
	Comp->SetMaterial(0, Material);
	Entry.PreviewActor->SetRootComponent(Comp);
	Entry.PreviewActor->SetActorLocation(FVector::ZeroVector);
	Entry.PreviewMeshComponent = Comp;

	ADirectionalLightActor* LightActor = Entry.PreviewWorld->SpawnActor<ADirectionalLightActor>();
	if (LightActor)
	{
		LightActor->InitDefaultComponents();
		LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));

		if (UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>())
		{
			LightComp->SetShadowBias(0.0f);
			LightComp->PushToScene();
		}
	}

	Entry.Viewport = std::make_unique<FStaticMeshEditorViewportClient>();
	Entry.Viewport->Initialize(Device, 128, 128);
	Entry.Viewport->SetPreviewWorld(Entry.PreviewWorld);
	Entry.Viewport->SetPreviewActor(Entry.PreviewActor);
	Entry.Viewport->SetPreviewMeshComponent(Comp);
	Entry.Viewport->ResetCameraToPreviewBounds();

	Entry.PreviewWorld->SetEditorPOVProvider(Entry.Viewport.get());
	Entry.WarmupFrames = 2;
	Entry.bReady = false;
}
