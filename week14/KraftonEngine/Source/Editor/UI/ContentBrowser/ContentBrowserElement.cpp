#include "ContentBrowserElement.h"

#include "Asset/AssetPackage.h"
#include "Editor/EditorEngine.h"
#include "Core/Logging/Log.h"
#include "FloatCurve/FloatCurveAsset.h"
#include "FloatCurve/FloatCurveManager.h"
#include "CameraShake/CameraShakeAsset.h"
#include "CameraShake/CameraShakeManager.h"
#include "Animation/Graph/AnimGraphAsset.h"
#include "Animation/Graph/AnimGraphManager.h"
#include "Platform/Paths.h"
#include "Serialization/SceneSaveManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Editor/UI/Dialog/PhysicsAssetCreationDialog.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Importer/FbxImporter.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Animation/Montage/AnimMontage.h"
#include "Animation/AnimationManager.h"
#include "Animation/Skeleton/Skeleton.h"
#include "Animation/Skeleton/SkeletonManager.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemManager.h"
#include "Physics/PhysicsAsset.h"
#include "Physics/PhysicsAssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Editor/UI/Dialog/FbxImportOptionsDialog.h"
#include "Editor/UI/Asset/Mesh/MeshEditorWidget.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <utility>

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

static bool GetTextureSizeForContentBrowser(ID3D11ShaderResourceView* Texture, ImVec2& OutSize)
{
	OutSize = ImVec2(0.0f, 0.0f);
	if (!Texture)
	{
		return false;
	}

	// SRV가 참조하는 실제 texture resource 조회
	ID3D11Resource* Resource = nullptr;
	Texture->GetResource(&Resource);
	if (!Resource)
	{
		return false;
	}

	ID3D11Texture2D* Texture2D = nullptr;
	const HRESULT Result = Resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&Texture2D));
	Resource->Release();

	if (FAILED(Result) || !Texture2D)
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC Desc {};
	Texture2D->GetDesc(&Desc);
	Texture2D->Release();

	if (Desc.Width == 0 || Desc.Height == 0)
	{
		return false;
	}

	OutSize = ImVec2(static_cast<float>(Desc.Width), static_cast<float>(Desc.Height));
	return true;
}

static void DrawContentBrowserImageAspectFit(ImDrawList* DrawList, ID3D11ShaderResourceView* Texture, const ImVec2& BoundsMin, const ImVec2& BoundsMax)
{
	if (!DrawList || !Texture)
	{
		return;
	}

	const ImVec2 BoundsSize(BoundsMax.x - BoundsMin.x, BoundsMax.y - BoundsMin.y);
	if (BoundsSize.x <= 0.0f || BoundsSize.y <= 0.0f)
	{
		return;
	}

	ImVec2 TextureSize;
	if (!GetTextureSizeForContentBrowser(Texture, TextureSize) || TextureSize.x <= 0.0f || TextureSize.y <= 0.0f)
	{
		// 크기 조회 실패 시 기존 stretch 출력으로 fallback
		DrawList->AddImage(Texture, BoundsMin, BoundsMax);
		return;
	}

	// 원본 비율 유지 후 카드 icon 영역 안에 최대 크기로 중앙 배치
	const float Scale = (std::min)(BoundsSize.x / TextureSize.x, BoundsSize.y / TextureSize.y);
	const ImVec2 ImageSize(TextureSize.x * Scale, TextureSize.y * Scale);
	const ImVec2 ImageMin(
		BoundsMin.x + (BoundsSize.x - ImageSize.x) * 0.5f,
		BoundsMin.y + (BoundsSize.y - ImageSize.y) * 0.5f);
	const ImVec2 ImageMax(ImageMin.x + ImageSize.x, ImageMin.y + ImageSize.y);

	DrawList->AddImage(Texture, ImageMin, ImageMax);
}

static std::filesystem::path ResolveProjectPathForContentBrowser(const FString& Path)
{
	std::filesystem::path FullPath(FPaths::ToWide(Path));
	if (!FullPath.is_absolute())
	{
		FullPath = std::filesystem::path(FPaths::RootDir()) / FullPath;
	}
	return FullPath.lexically_normal();
}

static bool ProjectFileExistsForContentBrowser(const FString& Path)
{
	const std::filesystem::path FullPath = ResolveProjectPathForContentBrowser(Path);
	return std::filesystem::exists(FullPath) && std::filesystem::is_regular_file(FullPath);
}

static void OpenContentItemInFileExplorer(const FContentItem& Item)
{
	if (Item.Path.empty())
	{
		return;
	}

	std::error_code Ec;
	const bool bIsDirectory = std::filesystem::is_directory(Item.Path, Ec);
	if (!Ec && bIsDirectory)
	{
		ShellExecuteW(nullptr, L"open", Item.Path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		return;
	}

	const std::wstring Parameters = L"/select,\"" + Item.Path.wstring() + L"\"";
	ShellExecuteW(nullptr, L"open", L"explorer.exe", Parameters.c_str(), nullptr, SW_SHOWNORMAL);
}

static bool IsSameOrChildPathForContentBrowser(const std::filesystem::path& Parent, const std::filesystem::path& Child)
{
	auto ParentIt = Parent.begin();
	auto ChildIt = Child.begin();

	for (; ParentIt != Parent.end() && ChildIt != Child.end(); ++ParentIt, ++ChildIt)
	{
		if (*ParentIt != *ChildIt)
		{
			return false;
		}
	}

	return ParentIt == Parent.end();
}

static bool HasImportedFbxAssetForContentBrowser(const FString& SourceFbxPath)
{
	return ProjectFileExistsForContentBrowser(FMeshManager::GetSkeletalMeshBinaryFilePath(SourceFbxPath)) ||
	ProjectFileExistsForContentBrowser(FMeshManager::GetStaticMeshBinaryFilePath(SourceFbxPath));
}

static bool TryOpenImportedFbxAssetForContentBrowser(ContentBrowserContext& Context, const FString& SourceFbxPath)
{
	if (!Context.EditorEngine)
	{
		return false;
	}

	ID3D11Device* Device = Context.EditorEngine->GetRenderer().GetFD3DDevice().GetDevice();

	const FString SkeletalPackagePath = FMeshManager::GetSkeletalMeshBinaryFilePath(SourceFbxPath);
	if (ProjectFileExistsForContentBrowser(SkeletalPackagePath))
	{
		if (USkeletalMesh* MeshAsset = FMeshManager::LoadSkeletalMesh(SkeletalPackagePath, Device))
		{
			FMeshEditorWidget::ClearImportDurationForAsset(MeshAsset->GetAssetPathFileName());
			Context.EditorEngine->OpenAssetEditorForObject(MeshAsset);
			return true;
		}
	}

	const FString StaticPackagePath = FMeshManager::GetStaticMeshBinaryFilePath(SourceFbxPath);
	if (ProjectFileExistsForContentBrowser(StaticPackagePath))
	{
		if (UStaticMesh* MeshAsset = FMeshManager::LoadStaticMesh(StaticPackagePath, Device))
		{
			Context.EditorEngine->OpenAssetEditorForObject(MeshAsset);
			return true;
		}
	}

	return false;
}

static bool ReimportOrImportStaticFbxForContentBrowser(ContentBrowserContext& Context, const FString& SourceFbxPath)
{
	if (!Context.EditorEngine)
	{
		return false;
	}

	ID3D11Device* Device            = Context.EditorEngine->GetRenderer().GetFD3DDevice().GetDevice();
	const FString StaticPackagePath = FMeshManager::GetStaticMeshBinaryFilePath(SourceFbxPath);

	if (ProjectFileExistsForContentBrowser(StaticPackagePath))
	{
		UStaticMesh* Reimported = nullptr;
		if (FMeshManager::ReimportStaticMesh(StaticPackagePath, Device, Reimported) && Reimported)
		{
			Context.bPendingContentRefresh = true;
			Context.EditorEngine->OpenAssetEditorForObject(Reimported);
			return true;
		}
		return false;
	}

	if (UStaticMesh* Imported = FMeshManager::LoadStaticMesh(SourceFbxPath, Device))
	{
		Context.bPendingContentRefresh = true;
		Context.EditorEngine->OpenAssetEditorForObject(Imported);
		return true;
	}

	return false;
}

static bool ImportFbxWithDefaultOptionsForContentBrowser(ContentBrowserContext& Context, const FString& SourceFbxPath)
{
	if (!Context.EditorEngine)
	{
		return false;
	}

	ID3D11Device* Device = Context.EditorEngine->GetRenderer().GetFD3DDevice().GetDevice();

	FString    ProbeMessage;
	const bool bHasSkin = FFbxImporter::HasSkinDeformer(SourceFbxPath, &ProbeMessage);
	if (!ProbeMessage.empty())
	{
		UE_LOG("FBX default import probe: Path=%s Message=%s", SourceFbxPath.c_str(), ProbeMessage.c_str());
	}

	if (bHasSkin)
	{
		FFbxSceneImportRequest Request;
		Request.SourceFbxPath            = SourceFbxPath;
		Request.bImportSkeleton          = true;
		Request.bImportSkin              = true;
		Request.bImportAnimations        = true;
		Request.bOverwriteExistingAssets = true;

		FFbxSceneImportResult Result;
		const auto            ImportStart = std::chrono::steady_clock::now();
		if (!FMeshManager::ImportFbxScene(Request, Device, Result))
		{
			return false;
		}

		if (Result.SkeletalMesh)
		{
			const std::chrono::duration<double> Elapsed = std::chrono::steady_clock::now() - ImportStart;
			FMeshEditorWidget::RecordImportDurationForAsset(
				Result.SkeletalMesh->GetAssetPathFileName(),
				Elapsed.count()
			);
			Context.bPendingContentRefresh = true;
			Context.EditorEngine->OpenAssetEditorForObject(Result.SkeletalMesh);
			return true;
		}

		return false;
	}

	if (UStaticMesh* MeshAsset = FMeshManager::LoadStaticMesh(SourceFbxPath, Device))
	{
		Context.bPendingContentRefresh = true;
		Context.EditorEngine->OpenAssetEditorForObject(MeshAsset);
		return true;
	}

	TArray<FFbxAnimationStackInfo> AnimationStacks;
	FString                        StackMessage;
	if (FFbxImporter::ListAnimationStacks(SourceFbxPath, AnimationStacks, &StackMessage) && !AnimationStacks.empty())
	{
		FFbxImportOptionsDialog::BeginSceneImport(Context.FbxImportDialog, SourceFbxPath);
		return true;
	}
	else if (!StackMessage.empty())
	{
		UE_LOG("FBX animation stack query failed: Path=%s Message=%s", SourceFbxPath.c_str(), StackMessage.c_str());
	}

	return false;
}

bool ContentBrowserElement::RenameTo(const FString& NewStem, FString* OutError)
{
	auto SetError = [&](const char* Msg) { if (OutError) *OutError = Msg; };

	if (NewStem.empty())
	{
		SetError("Name cannot be empty.");
		return false;
	}

	// Windows 금지 문자 차단 — 파일 시스템 에러 떨어지기 전에 명시적 메시지.
	static const char* kInvalidChars = "\\/:*?\"<>|";
	if (NewStem.find_first_of(kInvalidChars) != FString::npos)
	{
		SetError("Name contains invalid character (\\/:*?\"<>|).");
		return false;
	}

	const std::filesystem::path Dir = ContentItem.Path.parent_path();
	const std::wstring NewStemW = FPaths::ToWide(NewStem);

	// 파일은 확장자 유지, 디렉토리는 stem 자체가 곧 이름.
	std::filesystem::path NewPath;
	if (ContentItem.bIsDirectory)
	{
		NewPath = Dir / NewStemW;
	}
	else
	{
		NewPath = Dir / (NewStemW + ContentItem.Path.extension().wstring());
	}

	// 같은 path 면 no-op (성공 처리).
	if (NewPath == ContentItem.Path)
	{
		return true;
	}

	if (std::filesystem::exists(NewPath))
	{
		SetError("A file with that name already exists in this directory.");
		return false;
	}

	std::error_code Ec;
	std::filesystem::rename(ContentItem.Path, NewPath, Ec);
	if (Ec)
	{
		SetError(Ec.message().c_str());
		return false;
	}

	ContentItem.Path = NewPath;
	ContentItem.Name = NewPath.filename().wstring();
	return true;
}

bool ContentBrowserElement::DeleteFromDisk(FString* OutError)
{
	auto SetError = [&](const char* Msg) { if (OutError) *OutError = Msg; };

	if (ContentItem.Path.empty())
	{
		SetError("Path is empty.");
		return false;
	}

	std::error_code Ec;
	const bool bExists = std::filesystem::exists(ContentItem.Path, Ec);
	if (Ec)
	{
		SetError(Ec.message().c_str());
		return false;
	}
	if (!bExists)
	{
		SetError("Path does not exist.");
		return false;
	}

	const std::filesystem::path RootPath = std::filesystem::weakly_canonical(std::filesystem::path(FPaths::RootDir()), Ec);
	if (Ec)
	{
		SetError(Ec.message().c_str());
		return false;
	}

	const std::filesystem::path TargetPath = std::filesystem::weakly_canonical(ContentItem.Path, Ec);
	if (Ec)
	{
		SetError(Ec.message().c_str());
		return false;
	}

	if (TargetPath == RootPath)
	{
		SetError("Project root cannot be deleted.");
		return false;
	}

	if (!IsSameOrChildPathForContentBrowser(RootPath, TargetPath))
	{
		SetError("Path is outside the project root.");
		return false;
	}

	const bool bIsDirectory = std::filesystem::is_directory(TargetPath, Ec);
	if (Ec)
	{
		SetError(Ec.message().c_str());
		return false;
	}

	if (bIsDirectory)
	{
		const uintmax_t RemovedCount = std::filesystem::remove_all(TargetPath, Ec);
		if (Ec)
		{
			SetError(Ec.message().c_str());
			return false;
		}

		if (RemovedCount == 0)
		{
			SetError("No files were deleted.");
			return false;
		}

		return true;
	}

	const bool bRemoved = std::filesystem::remove(TargetPath, Ec);
	if (Ec)
	{
		SetError(Ec.message().c_str());
		return false;
	}

	if (!bRemoved)
	{
		SetError("File could not be deleted.");
		return false;
	}

	return true;
}

bool ContentBrowserElement::RenderSelectSpace(ContentBrowserContext& Context)
{
	FString Name = FPaths::ToUtf8(ContentItem.Name);
	ImGui::PushID(Name.c_str());

	bIsSelected = Context.SelectedElement.get() == this;

	const ImVec2 CardSize = Context.ContentSize;
	const bool bClicked = ImGui::InvisibleButton("##ElementCard", CardSize);

	const bool bHovered = ImGui::IsItemHovered();
	const bool bVisible = ImGui::IsItemVisible();

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
		ID3D11ShaderResourceView* DrawIcon = Icon;

		if (bVisible && bUseMeshThumbnail)
		{
			if (ID3D11ShaderResourceView* Thumbnail =
				FEditorMeshThumbnailManager::Get().GetOrRequestThumbnail(MeshThumbnailAssetPath, MeshThumbnailType))
			{
				DrawIcon = Thumbnail;
			}
		}

		if (bVisible && bUseMaterialThumbnail)
		{
			if (ID3D11ShaderResourceView* Thumbnail =
				FEditorMaterialThumbnailManager::Get().GetOrRequestThumbnail(MaterialThumbnailAssetPath))
			{
				DrawIcon = Thumbnail;
			}
		}

		if (DrawIcon && IconMax.y > IconMin.y)
		{
			DrawContentBrowserImageAspectFit(DrawList, DrawIcon, IconMin, IconMax);
		}
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
		// 모든 element 공통 — 자식 클래스의 RenderContextMenu 위에 Rename 항목 제공.
		// 클릭 시 이 element 를 selected 로 만들고 rename popup 요청 set — ContentBrowser
		// 가 다음 프레임 modal popup 열어 처리.
		if (ImGui::MenuItem("Rename"))
		{
			Context.SelectedElement = shared_from_this();
			Context.bRenameRequested = true;
		}
		if (ImGui::MenuItem("Delete"))
		{
			Context.SelectedElement = shared_from_this();
			Context.bDeleteRequested = true;
		}
		if (ImGui::MenuItem("Open In File Explorer"))
		{
			Context.SelectedElement = shared_from_this();
			OpenContentItemInFileExplorer(ContentItem);
		}
		ImGui::Separator();
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

void ContentBrowserElement::RenderListRow(ContentBrowserContext& Context)
{
	FString Name = FPaths::ToUtf8(ContentItem.Name);
	ImGui::PushID(Name.c_str());

	bIsSelected = Context.SelectedElement.get() == this;

	const char* TypeLabel = GetTypeLabel();
	const bool bHasTypeLabel = TypeLabel && TypeLabel[0] != '\0';
	const float FontSize = ImGui::GetFontSize();
	const float RowHeight = FontSize + ImGui::GetStyle().FramePadding.y * 2.0f + 2.0f;
	const float RowWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
	const bool bClicked = ImGui::InvisibleButton("##ElementListRow", ImVec2(RowWidth, RowHeight));
	const bool bHovered = ImGui::IsItemHovered();

	ImVec2 Min = ImGui::GetItemRectMin();
	ImVec2 Max = ImGui::GetItemRectMax();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const ImU32 RowColor = bIsSelected
		? IM_COL32(54, 86, 130, 255)
		: bHovered
		? IM_COL32(48, 50, 56, 255)
		: IM_COL32(0, 0, 0, 0);

	if (RowColor != IM_COL32(0, 0, 0, 0))
	{
		DrawList->AddRectFilled(Min, Max, RowColor, 3.0f);
	}

	const uint32 AccentColor = GetAccentColor();
	const float PaddingX = 8.0f;
	if (AccentColor != 0)
	{
		DrawList->AddRectFilled(
			ImVec2(Min.x, Min.y + 4.0f),
			ImVec2(Min.x + 3.0f, Max.y - 4.0f),
			AccentColor,
			2.0f);
	}

	const float TypeWidth = (bHasTypeLabel && RowWidth > 180.0f) ? 112.0f : 0.0f;
	const float NameMaxWidth = (std::max)(32.0f, RowWidth - TypeWidth - PaddingX * 3.0f);
	const FString DisplayName = GetDisplayName();
	const FString ClippedName = EllipsisText(DisplayName, NameMaxWidth);
	const ImVec2 TextPos(Min.x + PaddingX + 6.0f, Min.y + ImGui::GetStyle().FramePadding.y + 1.0f);
	DrawList->AddText(TextPos, ImGui::GetColorU32(ImGuiCol_Text), ClippedName.c_str());

	if (TypeWidth > 0.0f)
	{
		const ImVec2 TypePos(Max.x - TypeWidth, TextPos.y);
		DrawList->AddText(TypePos, ImGui::GetColorU32(ImGuiCol_TextDisabled), TypeLabel);
	}

	if (bHovered && ClippedName != DisplayName)
	{
		ImGui::SetTooltip("%s", DisplayName.c_str());
	}

	if (bClicked)
	{
		Context.SelectedElement = shared_from_this();
		bIsSelected = true;
		OnLeftClicked(Context);
	}

	if (ImGui::BeginPopupContextItem())
	{
		// 모든 element 공통 메뉴를 리스트 행에서도 카드와 동일하게 제공합니다.
		if (ImGui::MenuItem("Rename"))
		{
			Context.SelectedElement = shared_from_this();
			Context.bRenameRequested = true;
		}
		if (ImGui::MenuItem("Delete"))
		{
			Context.SelectedElement = shared_from_this();
			Context.bDeleteRequested = true;
		}
		if (ImGui::MenuItem("Open In File Explorer"))
		{
			Context.SelectedElement = shared_from_this();
			OpenContentItemInFileExplorer(ContentItem);
		}
		ImGui::Separator();
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
		ImGui::TextUnformatted(DisplayName.c_str());
		ImGui::SetDragDropPayload(GetDragItemType(), &ContentItem, sizeof(ContentItem));
		OnDrag(Context);
		ImGui::EndDragDropSource();
	}

	ImGui::PopID();
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

			if (Context.EditorEngine && FMeshManager::ReimportStaticMesh(
					PackagePath,
					Context.EditorEngine->GetRenderer().GetFD3DDevice().GetDevice(),
					Reimported
			) && Reimported)
			{
				Context.bPendingContentRefresh = true;
				Context.EditorEngine->OpenAssetEditorForObject(Reimported);
			}
		}
	}
}

void ObjectElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	if (!Context.EditorEngine)
	{
		ShellExecuteW(nullptr, L"open", ContentItem.Path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		return;
	}

	FString Extension = FPaths::ToUtf8(ContentItem.Path.extension());
	std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);

	const FString FilePath = FPaths::ToUtf8(ContentItem.Path.wstring());
	const FString PackagePath = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());

	if (Extension == ".uasset" && FMeshManager::IsStaticMeshPackage(PackagePath))
	{
		if (UStaticMesh* MeshAsset = FMeshManager::LoadStaticMesh(FilePath, Context.EditorEngine->GetRenderer().GetFD3DDevice().GetDevice()))
		{
			Context.EditorEngine->OpenAssetEditorForObject(MeshAsset);
		}
		return;
	}

	ShellExecuteW(nullptr, L"open", ContentItem.Path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
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

void AnimGraphElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	if (!Context.EditorEngine)
	{
		return;
	}
	const FString FilePath = FPaths::ToUtf8(ContentItem.Path.wstring());
	if (UAnimGraphAsset* GraphAsset = FAnimGraphManager::Get().Load(FilePath))
	{
		Context.EditorEngine->OpenAssetEditorForObject(GraphAsset);
	}
}

void MeshElement::RenderContextMenu(ContentBrowserContext& Context)
{
	FString Extension = FPaths::ToUtf8(ContentItem.Path.extension());
	std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);

	const FString FilePath = FPaths::ToUtf8(ContentItem.Path.wstring());
	FString PackagePath = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());

	if (Extension == ".fbx")
	{
		const bool bHasImportedAsset = HasImportedFbxAssetForContentBrowser(FilePath);
		if (bHasImportedAsset && ImGui::MenuItem("Open Imported Asset"))
		{
			TryOpenImportedFbxAssetForContentBrowser(Context, FilePath);
		}

		if (ImGui::MenuItem(bHasImportedAsset ? "Reimport Options..." : "Import Options..."))
		{
			FFbxImportOptionsDialog::BeginSceneImport(Context.FbxImportDialog, FilePath);

			if (!Context.FbxImportDialog.bHasSkin && Context.FbxImportDialog.AnimationStacks.empty())
			{
				Context.FbxImportDialog = FFbxSceneImportDialogState {};
				ReimportOrImportStaticFbxForContentBrowser(Context, FilePath);
			}
		}
		return;
	}

	if (Extension == ".uasset" && FMeshManager::IsSkeletalMeshPackage(PackagePath))
	{
		if (ImGui::MenuItem("Reimport"))
		{
			USkeletalMesh* Reimported = nullptr;

			const auto ReimportStart = std::chrono::steady_clock::now();
			if (Context.EditorEngine && FMeshManager::ReimportSkeletalMesh(
					PackagePath,
					Context.EditorEngine->GetRenderer().GetFD3DDevice().GetDevice(),
					Reimported
			) && Reimported)
			{
				const std::chrono::duration<double> Elapsed = std::chrono::steady_clock::now() - ReimportStart;
				FMeshEditorWidget::RecordImportDurationForAsset(
					Reimported->GetAssetPathFileName(),
					Elapsed.count()
				);
				Context.bPendingContentRefresh = true;
				Context.EditorEngine->OpenAssetEditorForObject(Reimported);
			}
		}

		if (ImGui::MenuItem("Create Physics Asset..."))
		{
			if (Context.EditorEngine)
			{
				USkeletalMesh* MeshAsset = FMeshManager::LoadSkeletalMesh(
					FilePath,
					Context.EditorEngine->GetRenderer().GetFD3DDevice().GetDevice()
				);

				if (MeshAsset)
				{
					const FString DirectoryPath = FPaths::ToUtf8(ContentItem.Path.parent_path().wstring());
					const FString DefaultName = FPaths::ToUtf8(ContentItem.Path.stem().wstring()) + "_PhysicsAsset";

					FPhysicsAssetCreationDialog::Begin(
						Context.PhysicsAssetDialog,
						MeshAsset,
						DirectoryPath,
						DefaultName
					);
				}
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
	FString Extension = FPaths::ToUtf8(ContentItem.Path.extension());
	std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);

	if (Extension == ".fbx")
	{
		if (TryOpenImportedFbxAssetForContentBrowser(Context, FilePath))
		{
			return;
		}

		ImportFbxWithDefaultOptionsForContentBrowser(Context, FilePath);
		return;
	}

	if (USkeletalMesh* MeshAsset = FMeshManager::LoadSkeletalMesh(FilePath, Context.EditorEngine->GetRenderer().GetFD3DDevice().GetDevice()))
	{
		FMeshEditorWidget::ClearImportDurationForAsset(MeshAsset->GetAssetPathFileName());
		Context.EditorEngine->OpenAssetEditorForObject(MeshAsset);
	}
}

static USkeletalMesh* ResolveCompatibleSkeletalMeshForBinding(ContentBrowserContext& Context, const FSkeletonBinding& Binding)
{
	if (!Context.EditorEngine)
	{
		return nullptr;
	}

	ID3D11Device* Device = Context.EditorEngine->GetRenderer().GetFD3DDevice().GetDevice();

	const TArray<FAssetListItem> Meshes = FAssetRegistry::ListMeshesForSkeleton(Binding, /*bAllowSameStructure=*/true);
	for (const FAssetListItem& Item : Meshes)
	{
		if (USkeletalMesh* Mesh = FMeshManager::LoadSkeletalMesh(Item.FullPath, Device))
		{
			return Mesh;
		}
	}

	return nullptr;
}

void AnimationElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	if (!Context.EditorEngine)
	{
		return;
	}

	const FString PackagePath = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());

	EAssetPackageType PackageType = EAssetPackageType::Unknown;
	if (!FAssetPackage::GetPackageType(PackagePath, PackageType))
	{
		return;
	}

	FSkeletonBinding Binding;
	if (PackageType == EAssetPackageType::AnimSequence)
	{
		UAnimSequence* Seq = FAnimationManager::Get().LoadAnimation(PackagePath);
		if (!Seq)
		{
			return;
		}
		Binding = Seq->GetSkeletonBinding();
	}
	else if (PackageType == EAssetPackageType::AnimMontage)
	{
		UAnimMontage* Montage = FAnimationManager::Get().LoadMontage(PackagePath);
		if (!Montage)
		{
			return;
		}
		if (const UAnimSequence* Src = Montage->GetSourceSequence())
		{
			Binding = Src->GetSkeletonBinding();
		}
	}
	else
	{
		return;
	}

	if (USkeletalMesh* Mesh = ResolveCompatibleSkeletalMeshForBinding(Context, Binding))
	{
		FMeshEditorWidget::ClearImportDurationForAsset(Mesh->GetAssetPathFileName());
		Context.EditorEngine->OpenAssetEditorForObject(Mesh);
	}
}

void SkeletonElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	if (!Context.EditorEngine)
	{
		return;
	}

	const FString PackagePath = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());

	USkeleton* Skeleton = FSkeletonManager::Get().LoadSkeleton(PackagePath);
	if (!Skeleton)
	{
		return;
	}

	if (USkeletalMesh* Mesh = ResolveCompatibleSkeletalMeshForBinding(Context, Skeleton->GetSkeletonBinding()))
	{
		FMeshEditorWidget::ClearImportDurationForAsset(Mesh->GetAssetPathFileName());
		Context.EditorEngine->OpenAssetEditorForObject(Mesh);
	}
}

void PhysicsAssetElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	if (!Context.EditorEngine)
	{
		return;
	}

	const FString FilePath = FPaths::ToUtf8(ContentItem.Path.wstring());
	if (UPhysicsAsset* Asset = FPhysicsAssetManager::Get().Load(FilePath))
	{
		Context.EditorEngine->OpenAssetEditorForObject(Asset);
	}
}

void ParticleSystemElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	if (!Context.EditorEngine)
	{
		return;
	}

	const FString FilePath = FPaths::ToUtf8(ContentItem.Path.wstring());
	if (UParticleSystem* Particle = FParticleSystemManager::Get().Load(FilePath))
	{
		Context.EditorEngine->OpenAssetEditorForObject(Particle);
	}
}

void RmlElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	if (!Context.EditorEngine)
	{
		ShellExecuteW(nullptr, L"open", ContentItem.Path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		return;
	}

	Context.EditorEngine->OpenUIEditor(ContentItem.Path);
}

void MaterialElement::OnDoubleLeftClicked(ContentBrowserContext& Context)
{
	if (!Context.EditorEngine)
	{
		return;
	}

	const FString PackagePath = FPaths::ToUtf8(
		ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring()
	);

	if (UMaterialInterface* Material = FMaterialManager::Get().GetOrCreateMaterialInterface(PackagePath))
	{
		Context.EditorEngine->OpenAssetEditorForObject(Material);
	}
}

static FString GetLowerExtensionForContentBrowser(const std::filesystem::path& Path)
{
	FString Extension = FPaths::ToUtf8(Path.extension());
	std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::tolower);
	return Extension;
}

static bool IsMaterialInstanceContentPath(const std::filesystem::path& Path)
{
	return GetLowerExtensionForContentBrowser(Path) == ".matinst";
}

const char* MaterialElement::GetTypeLabel() const
{
	return IsMaterialInstanceContentPath(ContentItem.Path)
		? "Material Instance"
		: "Material";
}

uint32 MaterialElement::GetAccentColor() const
{
	return IsMaterialInstanceContentPath(ContentItem.Path)
		? IM_COL32(130, 190, 255, 255)
		: IM_COL32(210, 170, 80, 255);
}
