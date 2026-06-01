#include "MeshEditorWidget.h"

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include "MeshEditorWidgetTab.h"
#include "MeshEditorWidgetTabs.h"

#include "Component/Light/DirectionalLightComponent.h"
#include "Editor/UI/Util/EditorTextureManager.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "Platform/Paths.h"
#include "Runtime/Engine.h"
#include "Slate/SlateApplication.h"

#include <imgui.h>

namespace
{
	ID3D11ShaderResourceView* LoadTabIcon(const wchar_t* FileName)
	{
		const FString Path = FPaths::ToUtf8(
			FPaths::Combine(FPaths::AssetDir(), L"Editor/ToolIcons/", FileName));
		return FEditorTextureManager::Get().GetOrLoadIcon(Path);
	}

	TMap<FString, double> GMeshImportDurationsByAssetPath;
}

static uint32 GNextMeshEditorInstanceId = 0;

FMeshEditorWidget::FMeshEditorWidget()
	: InstanceId(GNextMeshEditorInstanceId++)
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("MeshEditorPreview_" + Id);
	WindowIdSuffix = "###MeshEditor_" + Id;
	InitializeTabs();
}

FMeshEditorWidget::~FMeshEditorWidget() = default;

void FMeshEditorWidget::RecordImportDurationForAsset(const FString& AssetPath, double Seconds)
{
	if (AssetPath.empty() || AssetPath == "None" || Seconds < 0.0)
	{
		return;
	}

	GMeshImportDurationsByAssetPath[AssetPath] = Seconds;
}

void FMeshEditorWidget::ClearImportDurationForAsset(const FString& AssetPath)
{
	if (AssetPath.empty() || AssetPath == "None")
	{
		return;
	}

	GMeshImportDurationsByAssetPath.erase(AssetPath);
}

double FMeshEditorWidget::GetRecordedImportDurationForAsset(const FString& AssetPath)
{
	if (AssetPath.empty() || AssetPath == "None")
	{
		return -1.0;
	}

	auto It = GMeshImportDurationsByAssetPath.find(AssetPath);
	return It != GMeshImportDurationsByAssetPath.end() ? It->second : -1.0;
}

bool FMeshEditorWidget::CanEdit(UObject* Object) const
{
	if (!Object) return false;
	for (const std::unique_ptr<FMeshEditorWidgetTab>& Tab : Tabs)
	{
		if (Tab && Tab->CanEdit(Object))
		{
			return true;
		}
	}

	return false;
}

bool FMeshEditorWidget::IsEditingObject(UObject* Object) const
{
	if (FAssetEditorWidget::IsEditingObject(Object)) return true;
	if (!IsOpen() || !Object) return false;

	for (const std::unique_ptr<FMeshEditorWidgetTab>& Tab : Tabs)
	{
		if (Tab && Tab->IsEditingObject(Object))
		{
			return true;
		}
	}

	return false;
}

void FMeshEditorWidget::OnReuseForObject(UObject* Object)
{
	for (const std::unique_ptr<FMeshEditorWidgetTab>& Tab : Tabs)
	{
		if (Tab && Tab->ShouldActivateOnReuse(Object))
		{
			SetActiveTab(Tab->GetType());
			return;
		}
	}
}

void FMeshEditorWidget::Open(UObject* Object)
{
	EMeshEditorTab InitialTab = EMeshEditorTab::Skeleton;
	UObject* ObjectToEdit = Object;
	if (!ResolveOpenTarget(Object, ObjectToEdit, InitialTab))
	{
		return;
	}
	FAssetEditorWidget::Open(ObjectToEdit);

	CreatePreviewScene();
	InitializeViewportForPreview();
	ActivateInitialTab(InitialTab);
}

bool FMeshEditorWidget::ResolveOpenTarget(UObject* Object, UObject*& OutObjectToEdit, EMeshEditorTab& OutInitialTab) const
{
	OutObjectToEdit = nullptr;
	OutInitialTab = EMeshEditorTab::Skeleton;

	for (const std::unique_ptr<FMeshEditorWidgetTab>& Tab : Tabs)
	{
		if (Tab && Tab->ResolveOpenTarget(Object, OutObjectToEdit, OutInitialTab))
		{
			return OutObjectToEdit != nullptr;
		}
	}

	return false;
}

void FMeshEditorWidget::CreatePreviewScene()
{
	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	AActor* Actor = WorldContext.World->SpawnActor<AActor>();
	Actor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));
	UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>();
	LightComp->SetShadowBias(0.0f);
	LightComp->PushToScene();

	AStaticMeshActor* FloorActor = WorldContext.World->SpawnActor<AStaticMeshActor>();
	FloorActor->InitDefaultComponents("Content/Data/BasicShape/Cube.OBJ");
	FloorActor->SetActorLocation(FVector(0.0f, 0.0f, -0.05f));
	FloorActor->SetActorScale(FVector(10.0f, 10.0f, 0.02f));

	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(Actor);
	for (const std::unique_ptr<FMeshEditorWidgetTab>& Tab : Tabs)
	{
		if (Tab)
		{
			Tab->OnPreviewActorCreated(Actor);
		}
	}
	WorldContext.World->SetEditorPOVProvider(&ViewportClient);
}

void FMeshEditorWidget::InitializeViewportForPreview()
{
	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();

	ViewportClient.Initialize(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), static_cast<uint32>(ViewportSize.x), static_cast<uint32>(ViewportSize.y));

	ViewportClient.CreatePreviewGizmo();
	ViewportClient.ResetCameraToPreviousBounds();
	FSlateApplication::Get().RegisterViewport(&ViewportClient);
}

void FMeshEditorWidget::ActivateInitialTab(EMeshEditorTab InitialTab)
{
	ResetTabs();
	for (const std::unique_ptr<FMeshEditorWidgetTab>& Tab : Tabs)
	{
		if (Tab)
		{
			Tab->OnEditorOpened();
		}
	}

	ActiveTab = InitialTab;
	if (FMeshEditorWidgetTab* Active = GetActiveTab())
	{
		Active->OnInitialActivated();
	}
}

void FMeshEditorWidget::Close()
{
	for (const std::unique_ptr<FMeshEditorWidgetTab>& Tab : Tabs)
	{
		if (Tab)
		{
			Tab->OnEditorClosing();
		}
	}

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

void FMeshEditorWidget::Tick(float DeltaTime)
{
	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
	}

	if (FMeshEditorWidgetTab* Active = GetActiveTab())
	{
		Active->Tick(DeltaTime);
	}
}

void FMeshEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen())
	{
		OutClients.push_back(&ViewportClient);
	}
}

void FMeshEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (bPendingClose)
	{
		Close();
		bPendingClose = false;
		return;
	}
	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	bool bWindowOpen = true;
	FString VisibleTitle = "Mesh Editor";
	FString AssetPath;
	for (const std::unique_ptr<FMeshEditorWidgetTab>& Tab : Tabs)
	{
		if (Tab)
		{
			AssetPath = Tab->GetEditorTitleAssetPath();
			if (!AssetPath.empty())
			{
				break;
			}
		}
	}
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
	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

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

	RenderTabBar();
	ImGui::Separator();

	const float AvailableHeight = ImGui::GetContentRegionAvail().y;
	if (FMeshEditorWidgetTab* ActiveTabWidget = GetActiveTab())
	{
		ActiveTabWidget->Render(AvailableHeight);
	}

	ImGui::End();

	if (!bWindowOpen)
	{
		bPendingClose = true;
	}
}

void FMeshEditorWidget::SetActiveTab(EMeshEditorTab Tab)
{
	if (ActiveTab == Tab)
	{
		return;
	}

	const EMeshEditorTab PreviousTab = ActiveTab;
	if (FMeshEditorWidgetTab* Previous = GetActiveTab())
	{
		Previous->OnDeactivated(Tab);
	}
	ActiveTab = Tab;
	if (FMeshEditorWidgetTab* Active = GetActiveTab())
	{
		Active->OnActivated(PreviousTab);
	}
}

void FMeshEditorWidget::InitializeTabs()
{
	Tabs.clear();
	Tabs.push_back(std::make_unique<FMeshEditorSkeletonTab>(*this));
	Tabs.push_back(std::make_unique<FMeshEditorMeshTab>(*this));
	Tabs.push_back(std::make_unique<FMeshEditorAnimationTab>(*this));
	Tabs.push_back(std::make_unique<FMeshEditorPhysicsAssetTab>(*this));
}

void FMeshEditorWidget::ResetTabs()
{
	for (const std::unique_ptr<FMeshEditorWidgetTab>& Tab : Tabs)
	{
		Tab->Reset();
	}
}

FMeshEditorWidgetTab* FMeshEditorWidget::FindTab(EMeshEditorTab Tab) const
{
	for (const std::unique_ptr<FMeshEditorWidgetTab>& Candidate : Tabs)
	{
		if (Candidate && Candidate->GetType() == Tab)
		{
			return Candidate.get();
		}
	}
	return nullptr;
}

FMeshEditorWidgetTab* FMeshEditorWidget::GetActiveTab() const
{
	return FindTab(ActiveTab);
}

void FMeshEditorWidget::RenderTabBar()
{
	constexpr float BarHeight = 30.0f;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 BarPos = ImGui::GetCursorScreenPos();
	const float BarWidth = ImGui::GetContentRegionAvail().x;
	DrawList->AddRectFilled(BarPos, ImVec2(BarPos.x + BarWidth, BarPos.y + BarHeight),
		IM_COL32(38, 38, 38, 255));

	for (const std::unique_ptr<FMeshEditorWidgetTab>& Tab : Tabs)
	{
		if (!Tab)
		{
			continue;
		}

		const char* Label = Tab->GetLabel();
		const bool bActive = (ActiveTab == Tab->GetType());
		constexpr float IconSz = 18.0f;
		constexpr float PadX = 14.0f;
		constexpr float Gap = 8.0f;

		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const ImVec2 TextSz = ImGui::CalcTextSize(Label);
		const float Width = PadX + IconSz + Gap + TextSz.x + PadX;

		ImGui::InvisibleButton(Label, ImVec2(Width, BarHeight));
		const bool bHovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked())
		{
			SetActiveTab(Tab->GetType());
		}

		if (bActive || bHovered)
		{
			DrawList->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + BarHeight),
				bActive ? IM_COL32(41, 41, 41, 255) : IM_COL32(255, 255, 255, 20));
		}

		const float IconY = Pos.y + (BarHeight - IconSz) * 0.5f;
		if (ID3D11ShaderResourceView* Icon = LoadTabIcon(Tab->GetIconFileName()))
		{
			DrawList->AddImage(reinterpret_cast<ImTextureID>(Icon),
				ImVec2(Pos.x + PadX, IconY),
				ImVec2(Pos.x + PadX + IconSz, IconY + IconSz));
		}

		DrawList->AddText(ImVec2(Pos.x + PadX + IconSz + Gap, Pos.y + (BarHeight - TextSz.y) * 0.5f),
			bActive ? IM_COL32(255, 255, 255, 255) : IM_COL32(190, 190, 190, 255),
			Label);

		if (bActive)
		{
			DrawList->AddRectFilled(ImVec2(Pos.x, Pos.y + BarHeight - 2.0f),
				ImVec2(Pos.x + Width, Pos.y + BarHeight),
				IM_COL32(64, 132, 224, 255));
		}
		ImGui::SameLine(0.0f, 0.0f);
	}

	ImGui::NewLine();
}
