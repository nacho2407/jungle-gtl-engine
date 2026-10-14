#include "UI/Util/EditorMeshThumbnailManager.h"

#include "Mesh/MeshManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/Light/DirectionalLightComponent.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "GameFramework/World.h"
#include "Runtime/Engine.h"
#include "Viewport/Viewport.h"

void FEditorMeshThumbnailManager::Initialize(ID3D11Device* InDevice)
{
	Device = InDevice;
}

void FEditorMeshThumbnailManager::Shutdown()
{
	ClearThumbnails();
	Device = nullptr;
}

void FEditorMeshThumbnailManager::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	for (const auto& Pair : Entries)
	{
		const FMeshThumbnailEntry& Entry = *Pair.second;

		if (Entry.bReady)
		{
			continue;
		}

		if (Entry.StaticViewport && Entry.StaticViewport->IsRenderable())
		{
			OutClients.push_back(Entry.StaticViewport.get());
		}

		if (Entry.SkeletalViewport && Entry.SkeletalViewport->IsRenderable())
		{
			OutClients.push_back(Entry.SkeletalViewport.get());
		}
	}
}

void FEditorMeshThumbnailManager::ClearThumbnails()
{
	for (auto& Pair : Entries)
	{
		FMeshThumbnailEntry& Entry = *Pair.second;

		if (Entry.StaticViewport)
		{
			Entry.StaticViewport->Release();
		}

		if (Entry.SkeletalViewport)
		{
			Entry.SkeletalViewport->Release();
		}

		if (Entry.PreviewWorld && Entry.PreviewWorldHandle.IsValid())
		{
			GEngine->DestroyWorldContext(Entry.PreviewWorldHandle);
		}
	}

	Entries.clear();
	PendingRequests.clear();
}

ID3D11ShaderResourceView* FEditorMeshThumbnailManager::GetOrRequestThumbnail(const FString& AssetPath, EMeshThumbnailType Type)
{
	FMeshThumbnailEntry* Entry = FindOrCreateEntry(AssetPath, Type);
	if (!Entry) return nullptr;

	if (!Entry->bRequested)
	{
		Entry->bRequested = true;
		PendingRequests.push_back(MakeKey(AssetPath, Type));
	}

	FViewport* Viewport = nullptr;

	if (Entry->Type == EMeshThumbnailType::StaticMesh && Entry->StaticViewport)
	{
		Viewport = Entry->StaticViewport->GetViewport();
	}
	else if (Entry->Type == EMeshThumbnailType::SkeletalMesh && Entry->SkeletalViewport)
	{
		Viewport = Entry->SkeletalViewport->GetViewport();
	}

	return Entry->bReady && Viewport ? Viewport->GetSRV() : nullptr;
}

void FEditorMeshThumbnailManager::Tick(float DeltaTime)
{
	constexpr int32 MaxBuildPerFrame = 1;

	int32 BuiltCount = 0;
	while (!PendingRequests.empty() && BuiltCount < MaxBuildPerFrame)
	{
		FString Key = PendingRequests.front();
		PendingRequests.erase(PendingRequests.begin());

		auto It = Entries.find(Key);
		if (It == Entries.end()) continue;

		FMeshThumbnailEntry& Entry = *It->second;
		if (!Entry.PreviewWorld)
		{
			BuildPreviewScene(Entry);
			++BuiltCount;
		}
	}

	for (auto& Pair : Entries)
	{
		FMeshThumbnailEntry& Entry = *Pair.second;
		if (!Entry.PreviewWorld || Entry.bReady) continue;

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

FMeshThumbnailEntry* FEditorMeshThumbnailManager::FindOrCreateEntry(const FString& AssetPath, EMeshThumbnailType Type)
{
	FString Key = MakeKey(AssetPath, Type);

	if (auto It = Entries.find(Key); It != Entries.end())
	{
		return It->second.get();
	}

	auto NewEntry = std::make_unique<FMeshThumbnailEntry>();
	NewEntry->AssetPath = AssetPath;
	NewEntry->Type = Type;
	NewEntry->PreviewWorldHandle = FName("MeshThumbnail_" + std::to_string(Entries.size()));

	FMeshThumbnailEntry* Raw = NewEntry.get();
	Entries[Key] = std::move(NewEntry);
	return Raw;
}

void FEditorMeshThumbnailManager::BuildPreviewScene(FMeshThumbnailEntry& Entry)
{
	if (!Device || !GEngine)
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

	if (Entry.Type == EMeshThumbnailType::StaticMesh)
	{
		UStaticMesh* Mesh = FMeshManager::LoadStaticMesh(Entry.AssetPath, Device);
		if (!Mesh)
		{
			return;
		}

		UStaticMeshComponent* Comp = Entry.PreviewActor->AddComponent<UStaticMeshComponent>();
		Comp->SetStaticMesh(Mesh);
		Entry.PreviewActor->SetRootComponent(Comp);

		Entry.StaticViewport = std::make_unique<FStaticMeshEditorViewportClient>();
		Entry.StaticViewport->Initialize(Device, 128, 128);
		Entry.StaticViewport->SetPreviewWorld(Entry.PreviewWorld);
		Entry.StaticViewport->SetPreviewActor(Entry.PreviewActor);
		Entry.StaticViewport->SetPreviewMeshComponent(Comp);
		Entry.StaticViewport->ResetCameraToPreviewBounds();

		Entry.PreviewWorld->SetEditorPOVProvider(Entry.StaticViewport.get());
	}
	else
	{
		USkeletalMesh* Mesh = FMeshManager::LoadSkeletalMesh(Entry.AssetPath, Device);
		if (!Mesh)
		{
			return;
		}

		USkeletalMeshComponent* Comp = Entry.PreviewActor->AddComponent<USkeletalMeshComponent>();
		Comp->SetSkeletalMesh(Mesh);
		Entry.PreviewActor->SetRootComponent(Comp);

		Entry.SkeletalViewport = std::make_unique<FMeshEditorViewportClient>();
		Entry.SkeletalViewport->Initialize(Device, 128, 128);
		Entry.SkeletalViewport->SetPreviewWorld(Entry.PreviewWorld);
		Entry.SkeletalViewport->SetPreviewActor(Entry.PreviewActor);
		Entry.SkeletalViewport->SetPreviewMeshComponent(Comp);
		Entry.SkeletalViewport->ResetCameraToPreviousBounds();

		Entry.PreviewWorld->SetEditorPOVProvider(Entry.SkeletalViewport.get());
	}

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

	Entry.WarmupFrames = 2;
	Entry.bReady = false;
}

FString FEditorMeshThumbnailManager::MakeKey(const FString& AssetPath, EMeshThumbnailType Type)
{
	return AssetPath + (Type == EMeshThumbnailType::StaticMesh ? "#Static" : "#Skeletal");
}
