#include "ContentBrowserElement.h"

#include "Asset/AssetPackage.h"
#include "Editor/EditorEngine.h"
#include "FloatCurve/FloatCurveAsset.h"
#include "FloatCurve/FloatCurveManager.h"
#include "CameraShake/CameraShakeAsset.h"
#include "CameraShake/CameraShakeManager.h"
#include "Platform/Paths.h"
#include "Serialization/SceneSaveManager.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/MeshManager.h"

#include <algorithm>

static FString FormatBytes(uint64 Bytes)
{
	char Buffer[64];

	if (Bytes >= 1024ull * 1024ull)
	{
		std::snprintf(Buffer, sizeof(Buffer), "%.2f MB", static_cast<double>(Bytes) / (1024.0 * 1024.0));
	}
	else if (Bytes >= 1024ull)
	{
		std::snprintf(Buffer, sizeof(Buffer), "%.2f KB", static_cast<double>(Bytes) / 1024.0);
	}
	else
	{
		std::snprintf(Buffer, sizeof(Buffer), "%llu B", static_cast<unsigned long long>(Bytes));
	}

	return Buffer;
}

static void DrawDetailRow(const char* Label, const FString& Value)
{
	ImGui::TableNextRow();

	ImGui::TableSetColumnIndex(0);
	ImGui::TextDisabled("%s", Label);

	ImGui::TableSetColumnIndex(1);

	const float AvailableWidth = ImGui::GetContentRegionAvail().x;
	FString Clipped = Value;

	if (ImGui::CalcTextSize(Clipped.c_str()).x > AvailableWidth)
	{
		while (!Clipped.empty() && ImGui::CalcTextSize((Clipped + "...").c_str()).x > AvailableWidth)
		{
			Clipped.erase(Clipped.begin());
		}

		Clipped = "..." + Clipped;
	}

	ImGui::TextUnformatted(Clipped.c_str());

	if (ImGui::IsItemHovered() && Clipped != Value)
	{
		ImGui::SetTooltip("%s", Value.c_str());
	}
}

bool ContentBrowserElement::RenderSelectSpace(ContentBrowserContext& Context)
{
	FString Name = FPaths::ToUtf8(ContentItem.Name);
	ImGui::PushID(Name.c_str());

	bIsSelected = Context.SelectedElement.get() == this;

	const ImVec2 CardSize = Context.ContentSize;
	const bool bClicked = ImGui::InvisibleButton("##ElementCard", CardSize);

	const bool bHovered = ImGui::IsItemHovered();

	ImVec2 Min = ImGui::GetItemRectMin();
	ImVec2 Max = ImGui::GetItemRectMax();

	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const ImU32 CardColor = bIsSelected
		? IM_COL32(54, 86, 130, 255)
		: bHovered
		? IM_COL32(48, 50, 56, 255)
		: IM_COL32(34, 36, 40, 255);

	const ImU32 BorderColor = bIsSelected
		? IM_COL32(98, 160, 255, 255)
		: bHovered
		? IM_COL32(90, 94, 104, 255)
		: IM_COL32(55, 58, 64, 255);

	DrawList->AddRectFilled(Min, Max, CardColor, 6.0f);
	DrawList->AddRect(Min, Max, BorderColor, 6.0f, 0, bIsSelected ? 2.0f : 1.0f);

	const uint32 AccentColor = GetAccentColor();
	if (AccentColor != 0)
	{
		DrawList->AddRectFilled(
			ImVec2(Min.x, Min.y),
			ImVec2(Max.x, Min.y + 4.0f),
			AccentColor,
			6.0f,
			ImDrawFlags_RoundCornersTop);
	}

	const float Padding = 8.0f;
	const float FontSize = ImGui::GetFontSize();

	const float LabelHeight = FontSize * 2.4f;
	ImVec2 IconMin(Min.x + Padding, Min.y + Padding);
	ImVec2 IconMax(Max.x - Padding, Max.y - Padding - LabelHeight);

	if (Icon && IconMax.y > IconMin.y)
	{
		DrawList->AddImage(Icon, IconMin, IconMax);
	}

	const char* TypeLabel = GetTypeLabel();
	const bool bHasTypeLabel = TypeLabel && TypeLabel[0] != '\0';

	const FString DisplayName = EllipsisText(GetDisplayName(), CardSize.x - Padding * 2);

	ImVec2 TypePos(Min.x + Padding, Max.y - Padding - FontSize * 2.0f);
	ImVec2 NamePos(Min.x + Padding, Max.y - Padding - FontSize);

	if (bHasTypeLabel)
	{
		DrawList->AddText(TypePos, ImGui::GetColorU32(ImGuiCol_TextDisabled), TypeLabel);
	}

	DrawList->AddText(NamePos, ImGui::GetColorU32(ImGuiCol_Text), DisplayName.c_str());

	ImGui::PopID();

	return bClicked;
}

void ContentBrowserElement::Render(ContentBrowserContext& Context)
{
	if (RenderSelectSpace(Context))
	{
		Context.SelectedElement = shared_from_this();
		bIsSelected = true;
		OnLeftClicked(Context);
	}

	if (ImGui::BeginPopupContextItem())
	{
		RenderContextMenu(Context);
		ImGui::EndPopup();
	}

	bool bDoubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
	if (bDoubleClicked)
	{
		OnDoubleLeftClicked(Context);
	}

	if (ImGui::BeginDragDropSource())
	{
		RenderSelectSpace(Context);
		ImGui::SetDragDropPayload(GetDragItemType(), &ContentItem, sizeof(ContentItem));
		OnDrag(Context);
		ImGui::EndDragDropSource();
	}
}

void ContentBrowserElement::RenderDetail()
{
	const FString DisplayName = GetDisplayName();
	const char* TypeLabel = GetTypeLabel();

	ImGui::TextUnformatted(DisplayName.c_str());
	if (TypeLabel && TypeLabel[0] != '\0')
	{
		ImGui::TextDisabled("%s", TypeLabel);
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (ImGui::BeginTable("AssetDetailsTable", 2, ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 72.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		DrawDetailRow("Name", DisplayName);

		if (TypeLabel && TypeLabel[0] != '\0')
		{
			DrawDetailRow("Type", TypeLabel);
		}

		const FString RelativePath = FPaths::ToUtf8(
			ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());
		DrawDetailRow("Path", RelativePath);

		FString Extension = FPaths::ToUtf8(ContentItem.Path.extension());
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);

		if (Extension == ".uasset")
		{
			DrawDetailRow("Package", "uasset");

			const FString PackagePath = RelativePath;

			EAssetPackageType PackageType = EAssetPackageType::Unknown;
			if (FAssetPackage::GetPackageType(PackagePath, PackageType))
			{
				FAssetImportMetadata Metadata;
				if (FAssetPackage::ReadMetadata(PackagePath, PackageType, Metadata))
				{
					if (!Metadata.SourcePath.empty())
					{
						DrawDetailRow("Source", Metadata.SourcePath);
					}

					if (Metadata.SourceFileSize > 0)
					{
						DrawDetailRow("Size", FormatBytes(Metadata.SourceFileSize));
					}
				}
			}
		}

		ImGui::EndTable();
	}
}

FString ContentBrowserElement::EllipsisText(const FString& text, float maxWidth)
{
	ImFont* font = ImGui::GetFont();
	float fontSize = ImGui::GetFontSize();

	if (font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str()).x <= maxWidth)
		return text;

	const char* ellipsis = "...";
	float ellipsisWidth = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, ellipsis).x;

	std::string result = text;

	while (!result.empty())
	{
		result.pop_back();

		float w = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, result.c_str()).x;
		if (w + ellipsisWidth <= maxWidth)
		{
			result += ellipsis;
			break;
		}
	}

	return result;
}

FString ContentBrowserElement::GetDisplayName() const
{
	FString Extension = FPaths::ToUtf8(ContentItem.Path.extension());
	std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);

	if (Extension == ".uasset")
	{
		return FPaths::ToUtf8(ContentItem.Path.stem().wstring());
	}

	return FPaths::ToUtf8(ContentItem.Name);
}

void DirectoryElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	Context.CurrentPath = ContentItem.Path;
	Context.PendingRevealPath = ContentItem.Path;
	Context.bPendingContentRefresh = true;
}

void SceneElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	std::filesystem::path ScenePath = ContentItem.Path;
	FString FilePath = FPaths::ToUtf8(ScenePath.wstring());
	UEditorEngine* EditorEngine = Context.EditorEngine;
	EditorEngine->LoadSceneFromPath(FilePath);
}

void ObjectElement::RenderContextMenu(ContentBrowserContext& Context)
{
	FString Extension = FPaths::ToUtf8(ContentItem.Path.extension());
	std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);

	FString PackagePath = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());

	if (Extension == ".uasset" && FMeshManager::IsStaticMeshPackage(PackagePath))
	{
		if (ImGui::MenuItem("Reimport"))
		{
			UStaticMesh* Reimported = nullptr;

			if (Context.EditorEngine)
			{
				FMeshManager::ReimportStaticMesh(
					PackagePath,
					Context.EditorEngine->GetRenderer().GetFD3DDevice().GetDevice(),
					Reimported);
			}
		}
	}
}

void FloatCurveElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	if (!Context.EditorEngine)
	{
		return;
	}

	const FString FilePath = FPaths::ToUtf8(ContentItem.Path.wstring());
	if (UFloatCurveAsset* CurveAsset = FFloatCurveManager::Get().Load(FilePath))
	{
		Context.EditorEngine->OpenAssetEditorForObject(CurveAsset);
	}
}

void CameraShakeElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	if (!Context.EditorEngine)
	{
		return;
	}
	const FString FilePath = FPaths::ToUtf8(ContentItem.Path.wstring());
	if (UCameraShakeAsset* ShakeAsset = FCameraShakeManager::Get().Load(FilePath))
	{
		Context.EditorEngine->OpenAssetEditorForObject(ShakeAsset);
	}
}

void MeshElement::RenderContextMenu(ContentBrowserContext& Context)
{
	FString Extension = FPaths::ToUtf8(ContentItem.Path.extension());
	std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);

	FString PackagePath = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());

	if (Extension == ".uasset" && FMeshManager::IsSkeletalMeshPackage(PackagePath))
	{
		if (ImGui::MenuItem("Reimport"))
		{
			USkeletalMesh* Reimported = nullptr;

			if (Context.EditorEngine)
			{
				FMeshManager::ReimportSkeletalMesh(
					PackagePath,
					Context.EditorEngine->GetRenderer().GetFD3DDevice().GetDevice(),
					Reimported);
			}
		}
	}
}

void MeshElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	if (!Context.EditorEngine)
	{
		return;
	}
	const FString FilePath = FPaths::ToUtf8(ContentItem.Path.wstring());
	if (USkeletalMesh* MeshAsset = FMeshManager::LoadSkeletalMesh(FilePath, Context.EditorEngine->GetRenderer().GetFD3DDevice().GetDevice()))
	{
		Context.EditorEngine->OpenAssetEditorForObject(MeshAsset);
	}
}

void MaterialElement::OnLeftClicked(ContentBrowserContext& Context)
{
	MaterialInspector = { ContentItem.Path };
}

void MaterialElement::RenderDetail()
{
	MaterialInspector.Render();
}
