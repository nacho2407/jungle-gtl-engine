#include "Viewport/Services/EditorViewportAssetInteractionService.h"

#include "EditorEngine.h"
#include "Viewport/EditorViewportRegistry.h"
#include "Actor/Actor.h"
#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include "Picking/Picker.h"
#include "Renderer/Renderer.h"
#include "Serializer/SceneSerializer.h"
#include "Slate/SlateApplication.h"
#include "UI/EditorUI.h"
#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include "Component/StaticMeshComponent.h"
#include "Asset/ObjManager.h"

namespace
{
	bool HasStaticMeshAssetExtension(const FString& FilePath)
	{
		FString Extension = std::filesystem::path(FilePath).extension().string();
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char Ch)
		{
			return static_cast<char>(std::tolower(Ch));
		});

		return Extension == ".obj" || Extension == ".model";
	}
}

void FEditorViewportAssetInteractionService::HandleFileDoubleClick(
	FEditorUI& EditorUI,
	FEditorViewportRegistry& ViewportRegistry,
	const FString& FilePath) const
{
	FEditorEngine* Engine = EditorUI.GetEngine();
	if (!Engine || !FilePath.ends_with(".json"))
	{
		return;
	}

	Engine->SetSelectedActor(nullptr);
	Engine->GetScene()->ClearActors();

	FCameraSerializeData CameraData;
	const bool bLoaded = FSceneSerializer::Load(
		Engine->GetScene(),
		FilePath,
		Engine->GetRenderer()->GetDevice(),
		&CameraData);

	if (!bLoaded)
	{
		MessageBoxW(
			nullptr,
			L"Scene 정보가 올바르지 않습니다.",
			L"Error",
			MB_OK | MB_ICONWARNING);
		return;
	}

	if (CameraData.bValid)
	{
		FViewportEntry* PerspectiveEntry = nullptr;
		if (FSlateApplication* Slate = Engine->GetSlateApplication())
		{
			const FViewportId FocusedId = Slate->GetFocusedViewportId();
			if (FocusedId != INVALID_VIEWPORT_ID)
			{
				FViewportEntry* FocusedEntry = ViewportRegistry.FindEntryByViewportID(FocusedId);
				if (FocusedEntry &&
					FocusedEntry->bActive &&
					FocusedEntry->LocalState.ProjectionType == EViewportType::Perspective)
				{
					PerspectiveEntry = FocusedEntry;
				}
			}
		}
		if (!PerspectiveEntry)
		{
			PerspectiveEntry = ViewportRegistry.FindEntryByType(EViewportType::Perspective);
		}
		if (PerspectiveEntry)
		{
			PerspectiveEntry->LocalState.Position = CameraData.Location;
			PerspectiveEntry->LocalState.Rotation = CameraData.Rotation;
			PerspectiveEntry->LocalState.FovY = CameraData.FOV;
			PerspectiveEntry->LocalState.NearPlane = CameraData.NearClip;
			PerspectiveEntry->LocalState.FarPlane = CameraData.FarClip;
		}
	}

	UE_LOG("Scene loaded: %s", FilePath.c_str());
}

void FEditorViewportAssetInteractionService::HandleFileDropOnViewport(
	FEditorUI& EditorUI,
	const FPicker& Picker,
	const FEditorViewportRegistry& ViewportRegistry,
	int32 ScreenMouseX,
	int32 ScreenMouseY,
	const FString& FilePath) const
{
	FEditorEngine* Engine = EditorUI.GetEngine();
	if (!Engine || !Engine->GetRenderer() || !HasStaticMeshAssetExtension(FilePath))
	{
		return;
	}

	FSlateApplication* Slate = Engine->GetSlateApplication();
	if (!Slate)
	{
		return;
	}

	const FViewportEntry* Entry = ViewportRegistry.FindEntryByViewportID(Slate->GetFocusedViewportId());
	if (!Entry)
	{
		return;
	}

	const FRay Ray = Picker.ScreenToRay(*Entry, ScreenMouseX, ScreenMouseY);

	AActor* NewActor = Engine->GetScene()->SpawnActor<AActor>("DroppedObjActor");
	if (!NewActor)
	{
		return;
	}

	UStaticMeshComponent* MeshComponent = FObjectFactory::ConstructObject<UStaticMeshComponent>(NewActor);
	NewActor->AddOwnedComponent(MeshComponent);
	NewActor->SetRootComponent(MeshComponent);

	std::string PureFileName = std::filesystem::path(FilePath).filename().string();
	std::filesystem::path TargetPath = FPaths::MeshDir() / PureFileName;
	UStaticMesh* LoadedMesh = FObjManager::LoadStaticMeshAsset(TargetPath.string().c_str());

	if (LoadedMesh)
	{
		MeshComponent->SetStaticMesh(LoadedMesh);
	}
	else
	{
		UE_LOG("[에러] %s 로드 실패! 프로젝트의 Assets/Meshes 폴더에 파일이 있는지 확인하세요.", PureFileName.c_str());
	}

	FVector SpawnLocation = Ray.Origin + Ray.Direction * 5;
	NewActor->SetActorLocation(SpawnLocation);
	Engine->SetSelectedActor(NewActor);
}
