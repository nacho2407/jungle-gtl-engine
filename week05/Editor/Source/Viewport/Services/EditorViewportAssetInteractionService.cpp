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
#include "Component/StaticMeshComponent.h"
#include "Asset/ObjManager.h"

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
		FViewportEntry* PerspectiveEntry = ViewportRegistry.FindEntryByType(EViewportType::Perspective);
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
	if (!Engine || !Engine->GetRenderer() || !FilePath.ends_with(".obj"))
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

	UStaticMesh* LoadedMesh = FObjManager::LoadObjStaticMeshAsset(FPaths::ToRelativePath(FilePath));
	if (LoadedMesh) MeshComponent->SetStaticMesh(LoadedMesh);

	std::filesystem::path PngPath = FilePath;
	PngPath.replace_extension(".png");
	if (std::filesystem::exists(FPaths::ToAbsolutePath(PngPath.string())))
	{
		// TODO: 머티리얼 매니저를 통해 텍스처를 읽어오고 머티리얼을 생성해서 MeshComp에 입혀주는 로직 추가
		// 예: MeshComp->SetMaterial(0, 생성한머티리얼);
	}

	FVector SpawnLocation = Ray.Origin + Ray.Direction * 5;
	NewActor->SetActorLocation(SpawnLocation);
	Engine->SetSelectedActor(NewActor);
}
