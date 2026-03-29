#include "Viewport/Services/EditorViewportAssetInteractionService.h"

#include "EditorEngine.h"
#include "Viewport/EditorViewportRegistry.h"
#include "Actor/ObjActor.h"
#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include "Picking/Picker.h"
#include "Renderer/Renderer.h"
#include "Serializer/SceneSerializer.h"
#include "Slate/SlateApplication.h"
#include "UI/EditorUI.h"
#include <Windows.h>

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

	AObjActor* NewActor = Engine->GetScene()->SpawnActor<AObjActor>("ObjActor");
	if (!NewActor)
	{
		return;
	}

	NewActor->LoadObj(Engine->GetRenderer()->GetDevice(), FPaths::ToRelativePath(FilePath));
	NewActor->SetActorLocation(Ray.Origin + Ray.Direction * 5.0f);
}
