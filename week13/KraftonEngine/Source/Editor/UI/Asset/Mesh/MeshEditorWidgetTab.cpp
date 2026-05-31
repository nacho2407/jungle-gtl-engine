#include "MeshEditorWidgetTab.h"

#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Object/Object.h"
#include "Render/Shader/ShaderManager.h"
#include "Runtime/Engine.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"
#include "UI/Toolbar/ViewportToolbar.h"
#include "Viewport/Viewport.h"

#include <imgui.h>
#include <algorithm>
#include <cstdio>

namespace
{
	FString FormatMeshStatCount(size_t Value)
	{
		FString Result = std::to_string(Value);
		for (int32 InsertPos = static_cast<int32>(Result.length()) - 3; InsertPos > 0; InsertPos -= 3)
		{
			Result.insert(static_cast<size_t>(InsertPos), ",");
		}
		return Result;
	}

	FString FormatMeshStatSeconds(double Seconds)
	{
		char Buffer[64] = {};
		std::snprintf(Buffer, sizeof(Buffer), "%.3f sec", Seconds);
		return FString(Buffer);
	}

	EUberLitDefines::ELightingModel GetLightingModelForViewMode(EViewMode ViewMode)
	{
		switch (ViewMode)
		{
		case EViewMode::Unlit:
			return EUberLitDefines::ELightingModel::Unlit;
		case EViewMode::Lit_Gouraud:
			return EUberLitDefines::ELightingModel::Gouraud;
		case EViewMode::Lit_Lambert:
			return EUberLitDefines::ELightingModel::Lambert;
		case EViewMode::Lit_Phong:
		case EViewMode::LightCulling:
		default:
			return EUberLitDefines::ELightingModel::Phong;
		}
	}
}

FMeshEditorWidgetTab::FMeshEditorWidgetTab(FMeshEditorWidget& InOwner)
	: Owner(InOwner)
{
}

UObject* FMeshEditorWidgetTab::GetEditedObject() const
{
	return Owner.GetEditedObject();
}

USkeletalMesh* FMeshEditorWidgetTab::GetSkeletalMesh() const
{
	return Cast<USkeletalMesh>(Owner.GetEditedObject());
}

FMeshEditorViewportClient& FMeshEditorWidgetTab::GetViewportClient()
{
	return *Owner.GetViewportClient();
}

const FMeshEditorViewportClient& FMeshEditorWidgetTab::GetViewportClient() const
{
	return *Owner.GetViewportClient();
}

uint32 FMeshEditorWidgetTab::GetOwnerInstanceId() const
{
	return Owner.GetInstanceId();
}

void FMeshEditorWidgetTab::MarkDirty()
{
	Owner.MarkEditorDirty();
}

bool FMeshEditorWidgetTab::IsEditingCurrentSkeletalMesh(UObject* Object) const
{
	const USkeletalMesh* CurrentMesh = GetSkeletalMesh();
	const USkeletalMesh* RequestedMesh = Cast<USkeletalMesh>(Object);
	if (!CurrentMesh || !RequestedMesh)
	{
		return false;
	}

	const FString& CurrentPath = CurrentMesh->GetAssetPathFileName();
	return !CurrentPath.empty()
		&& CurrentPath != "None"
		&& CurrentPath == RequestedMesh->GetAssetPathFileName();
}

FString FMeshEditorWidgetTab::GetEditorTitleAssetPath() const
{
	const USkeletalMesh* SkeletalMesh = GetSkeletalMesh();
	return SkeletalMesh ? SkeletalMesh->GetAssetPathFileName() : FString();
}

void FMeshEditorWidgetTab::RenderViewportPanel(ImVec2 Size)
{
	FMeshEditorViewportClient& ViewportClient = GetViewportClient();
	ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
	ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y);

	FViewport* VP = ViewportClient.GetViewport();
	if (!VP || Size.x <= 0 || Size.y <= 0)
	{
		ImGui::Dummy(Size);
		return;
	}

	VP->RequestResize(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y));

	if (VP->GetSRV())
	{
		ImGui::Image((ImTextureID)VP->GetSRV(), Size);
	}
	else
	{
		ImGui::Dummy(Size);
	}

	FSlateApplication::Get().SetViewportImGuiHovered(&ViewportClient, ImGui::IsItemHovered());

	constexpr float ToolbarHeight = 28.0f;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(ViewportPos, ImVec2(ViewportPos.x + Size.x, ViewportPos.y + ToolbarHeight), IM_COL32(40, 40, 40, 255));

	FViewportToolbarContext Context;
	Context.Renderer = &GEngine->GetRenderer();
	Context.Gizmo = ViewportClient.GetGizmo();
	Context.Settings = &FEditorSettings::Get().MeshEditorViewportSettings;
	Context.RenderOptions = &ViewportClient.GetRenderOptions();
	Context.ToolbarLeft = ViewportPos.x;
	Context.ToolbarTop = ViewportPos.y;
	Context.ToolbarWidth = Size.x;
	Context.bReservePlayStopSpace = false;
	Context.bShowAddActor = false;
	Context.OnCoordSystemToggled = [&]()
	{
		FGizmoToolSettings& Settings = FEditorSettings::Get().MeshEditorViewportSettings.Gizmo;
		Settings.CoordSystem = (Settings.CoordSystem == EEditorCoordSystem::World) ? EEditorCoordSystem::Local : EEditorCoordSystem::World;
		ViewportClient.ApplyTransformSettingsToGizmo();
	};
	Context.OnSettingsChanged = [&]()
	{
		ViewportClient.ApplyTransformSettingsToGizmo();
	};
	Context.OnRenderViewModeExtras = [&]()
	{
		const EBoneDebugDrawMode CurrentBoneDrawMode = ViewportClient.GetBoneDebugDrawMode();
		int32 BoneDrawMode = static_cast<int32>(CurrentBoneDrawMode);
		ImGui::Text("Bone Display");
		ImGui::RadioButton("Selected Bone", &BoneDrawMode, static_cast<int32>(EBoneDebugDrawMode::SelectedOnly));
		ImGui::RadioButton("All Bones", &BoneDrawMode, static_cast<int32>(EBoneDebugDrawMode::AllBones));
		if (BoneDrawMode != static_cast<int32>(CurrentBoneDrawMode))
		{
			ViewportClient.SetBoneDebugDrawMode(static_cast<EBoneDebugDrawMode>(BoneDrawMode));
		}

		FViewportRenderOptions& RenderOptions = ViewportClient.GetRenderOptions();
		bool bWeightBoneHeatMap = RenderOptions.bWeightBoneHeatMap;
		if (ImGui::Checkbox("Weight Bone HeatMap", &bWeightBoneHeatMap))
		{
			RenderOptions.bWeightBoneHeatMap = bWeightBoneHeatMap;
			RenderOptions.WeightBoneHeatMapBoneIndex = GetSelectedBoneIndexForViewport();
			if (bWeightBoneHeatMap)
			{
				FShaderManager::Get().GetOrCreateUberLitPermutation(
					GetLightingModelForViewMode(RenderOptions.ViewMode),
					EUberLitDefines::EVertexFactory::SkeletalMesh,
					EShaderErrorMode::Notification,
					true);
			}
		}
	};

	FViewportToolbar::Render(Context);
	RenderMeshStatsOverlay(DrawList, ViewportPos);
}

void FMeshEditorWidgetTab::RenderMeshStatsOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos) const
{
	if (!DrawList || !GetEditedObject())
	{
		return;
	}

	size_t VertexCount = 0;
	size_t TriangleCount = 0;
	size_t IndexCount = 0;
	double ImportSeconds = -1.0;

	if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(GetEditedObject()))
	{
		if (const FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset())
		{
			VertexCount = Asset->Vertices.size();
			IndexCount = Asset->Indices.size();
			TriangleCount = Asset->Indices.size() / 3;
		}
		ImportSeconds = FMeshEditorWidget::GetRecordedImportDurationForAsset(SkeletalMesh->GetAssetPathFileName());
	}

	FString Text =
		"Triangles: " + FormatMeshStatCount(TriangleCount) + "\n" +
		"Vertices: " + FormatMeshStatCount(VertexCount) + "\n" +
		"Indices: " + FormatMeshStatCount(IndexCount);

	if (ImportSeconds >= 0.0)
	{
		Text += "\nImport Time: " + FormatMeshStatSeconds(ImportSeconds);
	}

	const ImVec2 TextPos(ViewportPos.x + 8.0f, ViewportPos.y + 36.0f);
	DrawList->AddText(ImVec2(TextPos.x + 1.0f, TextPos.y + 1.0f), IM_COL32(0, 0, 0, 220), Text.c_str());
	DrawList->AddText(TextPos, IM_COL32(235, 238, 242, 255), Text.c_str());
}
