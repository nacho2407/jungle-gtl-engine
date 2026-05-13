#include "StaticMeshEditorWidget.h"

#include "Component/Light/DirectionalLightComponent.h"
#include "Component/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "GameFramework/StaticMeshActor.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshAsset.h"
#include "Runtime/Engine.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"
#include "UI/Toolbar/ViewportToolbar.h"
#include "Viewport/Viewport.h"

#include <imgui.h>

namespace
{
	FString FormatStaticMeshStatCount(size_t Value)
	{
		FString Result = std::to_string(Value);
		for (int32 InsertPos = static_cast<int32>(Result.length()) - 3; InsertPos > 0; InsertPos -= 3)
		{
			Result.insert(static_cast<size_t>(InsertPos), ",");
		}
		return Result;
	}
}

static uint32 GNextStaticMeshEditorInstanceId = 0;

FStaticMeshEditorWidget::FStaticMeshEditorWidget()
	: InstanceId(GNextStaticMeshEditorInstanceId++)
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("StaticMeshEditorPreview_" + Id);
	WindowIdSuffix = "###StaticMeshEditor_" + Id;
}

bool FStaticMeshEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UStaticMesh>();
}

void FStaticMeshEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	AActor* Actor = WorldContext.World->SpawnActor<AActor>();
	if (UStaticMesh* Mesh = Cast<UStaticMesh>(EditedObject))
	{
		UStaticMeshComponent* Comp = Actor->AddComponent<UStaticMeshComponent>();
		Comp->SetStaticMesh(Mesh);
		Actor->SetRootComponent(Comp);
	}
	Actor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));
	UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>();
	LightComp->SetShadowBias(0.0f);
	LightComp->PushToScene();

	AStaticMeshActor* FloorActor = WorldContext.World->SpawnActor<AStaticMeshActor>();
	FloorActor->InitDefaultComponents("Data/BasicShape/Cube.OBJ");
	FloorActor->SetActorLocation(FVector(0.0f, 0.0f, -0.05f));
	FloorActor->SetActorScale(FVector(10.0f, 10.0f, 0.02f));

	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();

	ViewportClient.Initialize(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), ViewportSize.x, ViewportSize.y);
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(Actor);
	ViewportClient.SetPreviewMeshComponent(Actor->GetComponentByClass<UStaticMeshComponent>());
	ViewportClient.ResetCameraToPreviewBounds();

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);

	FSlateApplication::Get().RegisterViewport(&MeshViewportWindow, &ViewportClient);
}

void FStaticMeshEditorWidget::Close()
{
	FAssetEditorWidget::Close();

	if (UWorld* PreviewWorld = ViewportClient.GetPreviewWorld())
	{
		FScene& PreviewScene = PreviewWorld->GetScene();
		GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);

		if (PreviewWorldHandle.IsValid())
		{
			GEngine->DestroyWorldContext(PreviewWorldHandle);
		}
	}

	FSlateApplication::Get().UnregisterViewport(&ViewportClient);
	ViewportClient.Release();
}

void FStaticMeshEditorWidget::Tick(float DeltaTime)
{
	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
	}
}

void FStaticMeshEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen())
	{
		OutClients.push_back(const_cast<FStaticMeshEditorViewportClient*>(&ViewportClient));
	}
}

void FStaticMeshEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	static float DetailsWidth = 300.0f;
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(EditedObject);

	bool bWindowOpen = true;
	FString VisibleTitle = "Static Mesh Editor";
	const FString AssetPath = StaticMesh ? StaticMesh->GetAssetPathFileName() : FString();
	if (!AssetPath.empty())
	{
		VisibleTitle += " - ";
		VisibleTitle += AssetPath;
	}
	if (IsDirty())
	{
		VisibleTitle += " *";
	}

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_None;
	if (ViewportClient.IsMouseOverViewport())
	{
		WindowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
	}

	FString WindowTitle = VisibleTitle + WindowIdSuffix;
	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, WindowFlags))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		FSlateApplication::Get().BringViewportToFront(&ViewportClient);
	}

	ImGui::BeginGroup();
	{
		float AvailableWidth = ImGui::GetContentRegionAvail().x - DetailsWidth - ImGui::GetStyle().ItemSpacing.x;
		ImVec2 Size = ImVec2(AvailableWidth, ImGui::GetContentRegionAvail().y);

		ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
		ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y);

		FViewport* VP = ViewportClient.GetViewport();
		if (VP && Size.x > 0 && Size.y > 0)
		{
			VP->RequestResize(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y));
			MeshViewportWindow.SetRect(FRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y));

			if (VP->GetSRV())
			{
				ImGui::Image((ImTextureID)VP->GetSRV(), Size);
			}

			constexpr float ToolbarHeight = 28.0f;

			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			DrawList->AddRectFilled(ViewportPos,
				ImVec2(ViewportPos.x + Size.x, ViewportPos.y + ToolbarHeight),
				IM_COL32(40, 40, 40, 255));

			FViewportToolbarContext Context;
			Context.Renderer = &GEngine->GetRenderer();
			Context.Settings = &FEditorSettings::Get().MeshEditorViewportSettings;
			Context.RenderOptions = &ViewportClient.GetRenderOptions();
			Context.ToolbarLeft = ViewportPos.x;
			Context.ToolbarTop = ViewportPos.y;
			Context.ToolbarWidth = Size.x;
			Context.bReservePlayStopSpace = false;
			Context.bShowAddActor = false;
			Context.bShowGizmoControls = false;

			FViewportToolbar::Render(Context);
			RenderMeshStatsOverlay(DrawList, ViewportPos);
		}
	}
	ImGui::EndGroup();

	ImGui::SameLine();

	ImGui::BeginChild("Details", ImVec2(DetailsWidth, 0), true);
	ImGui::Text("Static Mesh Details");
	ImGui::Separator();
	RenderDetailsPanel(StaticMesh ? StaticMesh->GetStaticMeshAsset() : nullptr);
	ImGui::EndChild();

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}

void FStaticMeshEditorWidget::RenderMeshStatsOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos) const
{
	if (!DrawList || !EditedObject)
	{
		return;
	}

	size_t VertexCount = 0;
	size_t TriangleCount = 0;

	if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(EditedObject))
	{
		if (const FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset())
		{
			VertexCount = Asset->Vertices.size();
			TriangleCount = Asset->Indices.size() / 3;
		}
	}

	const FString Text =
		"Triangles: " + FormatStaticMeshStatCount(TriangleCount) + "\n" +
		"Vertices: " + FormatStaticMeshStatCount(VertexCount);

	const ImVec2 TextPos(ViewportPos.x + 8.0f, ViewportPos.y + 36.0f);
	DrawList->AddText(ImVec2(TextPos.x + 1.0f, TextPos.y + 1.0f), IM_COL32(0, 0, 0, 220), Text.c_str());
	DrawList->AddText(TextPos, IM_COL32(235, 238, 242, 255), Text.c_str());
}

void FStaticMeshEditorWidget::RenderDetailsPanel(FStaticMesh* Asset) const
{
	if (!Asset)
	{
		ImGui::TextDisabled("No static mesh data.");
		return;
	}

	ImGui::Text("Vertices: %s", FormatStaticMeshStatCount(Asset->Vertices.size()).c_str());
	ImGui::Text("Indices: %s", FormatStaticMeshStatCount(Asset->Indices.size()).c_str());
	ImGui::Text("Triangles: %s", FormatStaticMeshStatCount(Asset->Indices.size() / 3).c_str());
	ImGui::Text("Sections: %s", FormatStaticMeshStatCount(Asset->Sections.size()).c_str());
}
