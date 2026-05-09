#include "MeshEditorWidget.h"

#include "Mesh/StaticMesh.h"
#include "Mesh/SkeletalMesh.h"
#include "Runtime/Engine.h"
#include "Component/SkeletalMeshComponent.h"
#include "Viewport/Viewport.h"

#include <imgui.h>

bool FMeshEditorWidget::CanEdit(UObject* Object) const
{
	return Object && (Object->IsA<UStaticMesh>() || Object->IsA<USkeletalMesh>());
}

void FMeshEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, FName("MeshEditorPreview"));
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	AActor* Actor = WorldContext.World->SpawnActor<AActor>();
	if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(EditedObject))
	{
		USkeletalMeshComponent* Comp = Actor->AddComponent<USkeletalMeshComponent>();
		Comp->SetSkeletalMesh(Mesh);
		Actor->SetRootComponent(Comp);
	}

	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();

	ViewportClient.Initialize(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), ViewportSize.x, ViewportSize.y);
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(Actor);

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);
}

void FMeshEditorWidget::Close()
{
	FAssetEditorWidget::Close();

	GEngine->DestroyWorldContext(FName("MeshEditorPreview"));

	ViewportClient.Release();
}

void FMeshEditorWidget::Render(float DeltaTime)
{
	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(EditedObject);

	bool bWindowOpen = true;
	FString VisibleTitle = "Mesh Editor";
	if (!SkeletalMesh->GetAssetPathFileName().empty())
	{
		VisibleTitle += " - ";
		VisibleTitle += SkeletalMesh->GetAssetPathFileName();
	}
	if (IsDirty())
	{
		VisibleTitle += " *";
	}

	FString WindowTitle = VisibleTitle + "###MeshEditor";
	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	ImVec2 Size = ImGui::GetContentRegionAvail();

	ViewportClient.SetViewportRect(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y, Size.x, Size.y);

	FViewport* VP = ViewportClient.GetViewport();
	if (VP && Size.x > 0 && Size.y > 0)
	{
		VP->RequestResize(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y));

		if (VP->GetSRV())
		{
			ImGui::Image((ImTextureID)VP->GetSRV(), Size);
		}
	}

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}
