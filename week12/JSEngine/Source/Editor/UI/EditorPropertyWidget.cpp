#include "Editor/UI/EditorPropertyWidget.h"

#include "Editor/UI/ComponentMenuRegistry.h"
#include "Editor/EditorEngine.h"
#include "Editor/EditorRenderPipeline.h"
#include "ImGui/imgui.h"
#include "GameFramework/World.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/Movement/RotatingMovementComponent.h"
#include "Component/FireballComponent.h"
#include "Component/Movement/ProjectileMovementComponent.h"
#include "Component/Movement/InterpToMovementComponent.h"
#include "Component/Movement/PursuitMovementComponent.h"
#include "Component/ActorSequenceComponent.h"
#include "Component/PostProcess/Light/PointLightComponent.h"
#include "Core/EditorResourcePaths.h"
#include "Core/DebugDetails.h"
#include "Core/PropertyTypes.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "Core/Paths.h"
#include "Core/Containers/Set.h"
#include "Core/Logging/Log.h"
#include "Math/Color.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/Material.h"
#include "Asset/StaticMesh.h"
#include "Asset/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Object/FName.h"
#include "Object/Class.h"
#include "Object/Property.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <cstring>
#include "Component/HeightFogComponent.h"
#include "Selection/SelectionManager.h"
#include "Component/BoxComponent.h"
#include "Component/SphereComponent.h"
#include "Component/CapsuleComponent.h"
#include "Component/CameraComponent.h"
#include "Component/SpringArmComponent.h"
#include "Component/SoundComponent.h"
#include "Runtime/Script/ScriptManager.h"
#include <Runtime/Script/ScriptComponent.h>
#include <commdlg.h>
#include "Animation/AnimInstance.h"
#include "Animation/AnimationStateMachine.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

namespace UIConstants
{
	constexpr float XButtonSize    = 20.0f;
	constexpr float MinScrollHeight = 50.0f;
}

namespace
{
	using FDetailsPerfClock = std::chrono::steady_clock;

	double DetailsPerfMs(FDetailsPerfClock::time_point Start, FDetailsPerfClock::time_point End)
	{
		return std::chrono::duration<double, std::milli>(End - Start).count();
	}

	const TArray<FString>& EmptyAssetNames()
	{
		static const TArray<FString> Empty;
		return Empty;
	}

	static bool DrawXButton(const char* id, float size = UIConstants::XButtonSize)
	{
		ImGui::PushID(id);

		ImVec2 pos = ImGui::GetCursorScreenPos();
		bool bClicked = ImGui::InvisibleButton("##xbtn", ImVec2(size, size));

		ImVec4 col = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
		if      (ImGui::IsItemActive())  col = ImVec4(0.9f, 0.1f, 0.1f, 1.0f);
		else if (ImGui::IsItemHovered()) col = ImVec4(0.8f, 0.2f, 0.2f, 0.8f);

		ImDrawList* dl = ImGui::GetWindowDrawList();

		// 호버/클릭 시 배경 원
		ImVec2 center(pos.x + size * 0.5f + 0.5f, pos.y + size * 0.5f + 0.5f);
		dl->AddCircleFilled(center, size * 0.5f, ImGui::ColorConvertFloat4ToU32(
			ImGui::IsItemActive()
				? ImVec4(0.9f, 0.1f, 0.1f, 1.0f)
				: ImVec4(0.8f, 0.2f, 0.2f, 0.8f)));

		// X 직접 그리기 (폰트 무관)
		float pad = size * 0.3f;
		ImU32 color = ImGui::ColorConvertFloat4ToU32(col);
		dl->AddLine(
			ImVec2(pos.x + pad,        pos.y + pad),
			ImVec2(pos.x + size - pad, pos.y + size - pad),
			color, 2.0f);
		dl->AddLine(
			ImVec2(pos.x + size - pad, pos.y + pad),
			ImVec2(pos.x + pad,        pos.y + size - pad),
			color, 2.0f);

		ImGui::PopID();
		return bClicked;
	}

	static void DrawDetailsSeparator()
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
	}

	static void DrawDetailsSectionLabel(const char* Label)
	{
		ImVec2 Pos = ImGui::GetCursorScreenPos();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImU32 Color = ImGui::GetColorU32(ImGuiCol_Text);
		DrawList->AddText(ImVec2(Pos.x + 0.75f, Pos.y), Color, Label);
		ImGui::TextUnformatted(Label);
	}

	static const char* GetPropertyDisplayName(const FProperty& Prop)
	{
		return (Prop.DisplayName && Prop.DisplayName[0] != '\0') ? Prop.DisplayName : Prop.Name;
	}

	static bool IsSkeletalMeshSectionProperty(const FProperty* Property)
	{
		return Property
			&& Property->Name
			&& (std::strcmp(Property->Name, "SkeletalMeshPath") == 0
				|| std::strcmp(Property->Name, "SkinningMode") == 0
				|| std::strcmp(Property->Name, "AnimationAssetPath") == 0
				|| std::strcmp(Property->Name, "AnimGraphAssetPath") == 0
				|| std::strcmp(Property->Name, "AnimationMode") == 0);
	}

	static TArray<FString> CollectAnimGraphAssetPaths()
	{
		TArray<FString> Result;
		const std::filesystem::path AssetRoot = (std::filesystem::path(FPaths::RootDir()) / L"Asset").lexically_normal();
		if (!std::filesystem::exists(AssetRoot))
		{
			return Result;
		}

		std::error_code ErrorCode;
		for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(AssetRoot, ErrorCode))
		{
			if (ErrorCode)
			{
				break;
			}
			if (!Entry.is_regular_file())
			{
				continue;
			}

			FString Extension = FPaths::ToUtf8(Entry.path().extension().wstring());
			std::transform(Extension.begin(), Extension.end(), Extension.begin(),
				[](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
			if (Extension == ".animgraph")
			{
				Result.push_back(FPaths::Normalize(FPaths::ToRelativeString(Entry.path().wstring())));
			}
		}

		std::sort(Result.begin(), Result.end());
		return Result;
	}

	static bool RenderAnimGraphAssetPathWidget(FString& Value, const char* Label)
	{
		bool bChanged = false;
		TArray<FString> Options = CollectAnimGraphAssetPaths();
		const char* Preview = Value.empty() ? "<None>" : Value.c_str();

		if (ImGui::BeginCombo(Label, Preview))
		{
			if (ImGui::Selectable("<None>", Value.empty()))
			{
				Value.clear();
				bChanged = true;
			}
			for (const FString& Path : Options)
			{
				const bool bSelected = Value == Path;
				if (ImGui::Selectable(Path.c_str(), bSelected))
				{
					Value = Path;
					bChanged = true;
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("AnimGraphContentItem"))
			{
				if (Payload->Data && Payload->DataSize > 0)
				{
					const FString PayloadPath = static_cast<const char*>(Payload->Data);
					const std::filesystem::path DroppedPath = FPaths::ToWide(PayloadPath);
					Value = DroppedPath.is_absolute()
						? FPaths::Normalize(FPaths::ToRelativeString(DroppedPath.wstring()))
						: FPaths::Normalize(PayloadPath);
					bChanged = true;
				}
			}
			ImGui::EndDragDropTarget();
		}

		return bChanged;
	}

	static FString MakePropertyWidgetLabel(const FProperty& Prop)
	{
		const char* DisplayName = GetPropertyDisplayName(Prop);
		if (!DisplayName)
		{
			return "";
		}
		if (!Prop.Name || strcmp(DisplayName, Prop.Name) == 0)
		{
			return DisplayName;
		}
		return FString(DisplayName) + "##" + Prop.Name;
	}

	static void CollectEditableReflectedProperties(UObject* Object, TArray<const FProperty*>& OutProperties)
	{
		if (!Object || !Object->GetClass())
		{
			return;
		}

		TArray<const FProperty*> AllProperties;
		Object->GetClass()->GetAllProperties(AllProperties);
		for (const FProperty* Property : AllProperties)
		{
			if (!Property || !Property->Name || !Property->IsEditable())
			{
				continue;
			}
			OutProperties.push_back(Property);
		}
	}

	static int64 ReadEnumPropertyValue(const FProperty& Prop, const UObject* Object)
	{
		const void* ValuePtr = Prop.GetValuePtr(Object);
		if (!ValuePtr || !Prop.EnumMeta)
		{
			return 0;
		}

		switch (Prop.EnumMeta->Size)
		{
		case 1: return static_cast<int64>(*static_cast<const uint8*>(ValuePtr));
		case 2: return static_cast<int64>(*static_cast<const uint16*>(ValuePtr));
		case 4: return static_cast<int64>(*static_cast<const int32*>(ValuePtr));
		case 8: return static_cast<int64>(*static_cast<const int64*>(ValuePtr));
		default: return 0;
		}
	}

	static void WriteEnumPropertyValue(const FProperty& Prop, UObject* Object, int64 Value)
	{
		void* ValuePtr = Prop.GetValuePtr(Object);
		if (!ValuePtr || !Prop.EnumMeta)
		{
			return;
		}

		switch (Prop.EnumMeta->Size)
		{
		case 1: *static_cast<uint8*>(ValuePtr) = static_cast<uint8>(Value); break;
		case 2: *static_cast<uint16*>(ValuePtr) = static_cast<uint16>(Value); break;
		case 4: *static_cast<int32*>(ValuePtr) = static_cast<int32>(Value); break;
		case 8: *static_cast<int64*>(ValuePtr) = static_cast<int64>(Value); break;
		default: break;
		}
	}

	static bool IsLiveActor(AActor* Actor)
	{
		return Actor
			&& UObjectManager::Get().ContainsObject(Actor)
			&& !Actor->IsPendingKill();
	}

	static bool IsLiveComponent(UActorComponent* Component)
	{
		return Component && UObjectManager::Get().ContainsObject(Component);
	}

	// 컴포넌트 포인터를 ImGui PushID 용 문자열로 변환
	static void MakeXButtonId(char* OutBuf, size_t BufSize, const void* Ptr)
	{
		snprintf(OutBuf, BufSize, "xbtn_%p", Ptr);
	}

	static FString GetMovementComponentDisplayName(UMovementComponent* MoveComp)
	{
		if (!MoveComp) return "None";

		USceneComponent* UpdatedComp = MoveComp->GetUpdatedComponent();
		if (UpdatedComp)
		{
			FString TargetName = UpdatedComp->GetFName().ToString();
			if (TargetName.empty())
			{
				TargetName = UpdatedComp->GetClassName();
			}
			return FString("MC_") + TargetName;
		}

		// 대상이 없는 경우
		FString DefaultName = MoveComp->GetFName().ToString();
		if (DefaultName.empty())
		{
			DefaultName = MoveComp->GetClassName();
		}
		return DefaultName;
	}

	static FString MakeDefaultScriptName(const FString& SceneName, AActor* Actor)
	{
		FString ActorName = "Actor";
		FString ValidSceneName = SceneName.empty() ? "Default" : SceneName;

		if (Actor)
		{
			ActorName = Actor->GetClassName();
		}

		return ValidSceneName + "_" + ActorName;
	}

	static bool IsBlankString(const FString& Value)
	{
		return std::all_of(
			Value.begin(),
			Value.end(),
			[](unsigned char Ch)
			{
				return std::isspace(Ch) != 0;
			});
	}

	static FString MakeScriptReferenceFromPath(const FString& PathText)
	{
		if (PathText.empty())
		{
			return {};
		}

		std::filesystem::path ScriptPath(FPaths::ToWide(PathText));
		if (ScriptPath.is_absolute())
		{
			return FPaths::ToRelativeString(ScriptPath.lexically_normal().wstring());
		}
		return FPaths::Normalize(PathText);
	}

	static bool PromptCreateScriptAs(UEditorEngine* EditorEngine, const FString& ScriptPathHint, FString& OutFilePath)
	{
		OutFilePath.clear();

		std::filesystem::path ScriptDir = (std::filesystem::path(FPaths::RootDir()) / L"Asset" / L"Script").lexically_normal();
		std::error_code CreateDirEc;
		std::filesystem::create_directories(ScriptDir, CreateDirEc);

		std::filesystem::path HintPath(FPaths::ToWide(ScriptPathHint));
		if (HintPath.has_filename() && HintPath.extension() != L".lua")
		{
			HintPath.replace_extension(L".lua");
		}

		std::wstring FileName = HintPath.has_filename() ? HintPath.filename().wstring() : L"NewScript.lua";
		if (FileName.empty() || FileName == L".lua")
		{
			FileName = L"NewScript.lua";
		}

		std::filesystem::path InitialDir = ScriptDir;
		if (HintPath.has_parent_path())
		{
			std::filesystem::path CandidateDir = HintPath.is_absolute()
				? HintPath.parent_path()
				: (std::filesystem::path(FPaths::RootDir()) / HintPath.parent_path()).lexically_normal();
			std::error_code ExistsEc;
			if (std::filesystem::is_directory(CandidateDir, ExistsEc))
			{
				InitialDir = CandidateDir;
			}
		}

		WCHAR FileBuffer[MAX_PATH] = {};
		const std::wstring DefaultFile = (InitialDir / FileName).wstring();
		const std::wstring InitialDirString = InitialDir.wstring();
		wcsncpy_s(FileBuffer, MAX_PATH, DefaultFile.c_str(), _TRUNCATE);

		OPENFILENAMEW DialogDesc = {};
		DialogDesc.lStructSize = sizeof(DialogDesc);
		DialogDesc.hwndOwner = EditorEngine && EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr;
		DialogDesc.lpstrFilter = L"Lua Script Files (*.lua)\0*.lua\0All Files (*.*)\0*.*\0";
		DialogDesc.lpstrFile = FileBuffer;
		DialogDesc.nMaxFile = MAX_PATH;
		DialogDesc.lpstrInitialDir = InitialDirString.c_str();
		DialogDesc.lpstrDefExt = L"lua";
		DialogDesc.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

		if (!GetSaveFileNameW(&DialogDesc))
		{
			return false;
		}

		std::filesystem::path SelectedPath(FileBuffer);
		if (SelectedPath.extension() != L".lua")
		{
			SelectedPath.replace_extension(L".lua");
		}
		OutFilePath = FPaths::ToUtf8(SelectedPath.lexically_normal().wstring());
		return true;
	}

	static void InitializeSpawnedComponentDefaults(UActorComponent* Component)
	{
		if (USubUVComponent* SubUV = Cast<USubUVComponent>(Component))
		{
			SubUV->SetParticle(FName("Explosion"));
			SubUV->SetSpriteSize(2.0f, 2.0f);
			SubUV->SetFrameRate(30.f);
		}
		else if (UTextRenderComponent* Text = Cast<UTextRenderComponent>(Component))
		{
			Text->SetFont(FName("Default"));
			Text->SetText("TextRender");
		}
		else if (UBillboardComponent* Billboard = Cast<UBillboardComponent>(Component))
		{
			Billboard->SetTextureName(FEditorResourcePaths::Icon("Pawn_64x.png"));
		}
		else if (USpringArmComponent* SpringArm = Cast<USpringArmComponent>(Component))
		{
			SpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 1.6f));
		}
		else if (UHeightFogComponent* HeightFog = Cast<UHeightFogComponent>(Component))
		{
			HeightFog->SetFogDensity(0);
			HeightFog->SetFogInscatteringColor(FVector4(0.72f, 0.8f, 0.9f, 1.0f));
			HeightFog->SetHeightFalloff(0);
			HeightFog->SetFogHeight(0);
		}
	}
}

void FEditorPropertyWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
	ActorSequenceDetails.Initialize(EditorEngine, &bPropertyEditUndoCaptured);
}

void FEditorPropertyWidget::ResetSelection()
{
	SelectedComponent = nullptr;
	LastSelectedActor = nullptr;
	LockedDetailsActor = nullptr;
	bDetailsLocked = false;
	bActorSelected = true;
}

void FEditorPropertyWidget::Render(float DeltaTime)
{
	LastDeltaTime = DeltaTime;

	ImGui::SetNextWindowSize(ImVec2(350.0f, 500.0f), ImGuiCond_Once);
	ImGui::Begin("Details");

	const FWorldContext* Ctx = EditorEngine->GetFocusedWorldContext();

	AActor* CurrentSelection = Ctx->SelectionManager->GetPrimarySelection();
	if (!IsLiveActor(CurrentSelection))
	{
		Ctx->SelectionManager->ClearSelection();
		CurrentSelection = nullptr;
	}

	if (bDetailsLocked && LockedDetailsActor)
	{
		UWorld* LockedWorld = IsLiveActor(LockedDetailsActor) ? LockedDetailsActor->GetFocusedWorld() : nullptr;
		bool bLockedActorAlive = false;
		if (LockedWorld)
		{
			const TArray<AActor*>& WorldActors = LockedWorld->GetActors();
			bLockedActorAlive = std::find(WorldActors.begin(), WorldActors.end(), LockedDetailsActor) != WorldActors.end();
		}
		if (!bLockedActorAlive)
		{
			LockedDetailsActor = nullptr;
			bDetailsLocked = false;
			SelectedComponent = nullptr;
			LastSelectedActor = nullptr;
			bActorSelected = true;
		}
	}

	AActor* PrimaryActor = (bDetailsLocked && LockedDetailsActor) ? LockedDetailsActor : CurrentSelection;
	RenderDetailsLockBar(CurrentSelection, PrimaryActor);

	if (!IsLiveActor(PrimaryActor))
	{
		SelectedComponent = nullptr;
		LastSelectedActor = nullptr;
		bActorSelected = true;
		ImGui::Text("No object selected.");
		ImGui::End();
		return;
	}

	UpdateSelectionState(PrimaryActor);

	const TArray<AActor*>& SelectedActors = Ctx->SelectionManager->GetSelectedActors();
	TArray<AActor*> LockedActorList;
	const TArray<AActor*>* DisplayActors = &SelectedActors;
	if (bDetailsLocked)
	{
		LockedActorList.push_back(PrimaryActor);
		DisplayActors = &LockedActorList;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
		&& !ImGui::GetIO().WantTextInput
		&& ImGui::IsKeyPressed(ImGuiKey_Delete, false))
	{
		if (bActorSelected)
		{
			TArray<AActor*> ActorsToDelete = *DisplayActors;
			if (EditorEngine && EditorEngine->DeleteActors(ActorsToDelete) > 0)
			{
				SelectedComponent = nullptr;
				LastSelectedActor = nullptr;
				bActorSelected = true;
				ImGui::End();
				return;
			}
		}
		else
		{
			DeleteSelectedComponent(PrimaryActor);
		}
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
		&& !ImGui::GetIO().WantTextInput
		&& ImGui::IsKeyPressed(ImGuiKey_F2, false))
	{
		if (!bActorSelected && SelectedComponent)
		{
			bFocusComponentNameNextFrame = true;
		}
		else
		{
			SelectActorForDetails();
			bFocusActorNameNextFrame = true;
		}
	}

	// 상단 액터 정보 및 컨트롤 영역
	RenderActorHeaderRegion(PrimaryActor, *DisplayActors);

	if (!bDetailsLocked && Ctx->SelectionManager->GetPrimarySelection() == nullptr)
	{
		ImGui::End();
		return;
	}

	// 컴포넌트 트리 영역
	SEPARATOR();
	RenderComponentTree(PrimaryActor);
	RenderDetailsContextMenu(PrimaryActor, *DisplayActors);

	// 디테일 프로퍼티 영역
	SEPARATOR();
	DrawDetailsSectionLabel("Details");
	DrawDetailsSeparator();

	float ScrollHeight = std::max(UIConstants::MinScrollHeight, ImGui::GetContentRegionAvail().y);
	ImGui::BeginChild("##Details", ImVec2(0, ScrollHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	{
		RenderDetails(PrimaryActor, *DisplayActors);
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)
			&& ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		{
			bOpenDetailsContextMenu = true;
		}
	}
	ImGui::EndChild();

	ImGui::End();
}

void FEditorPropertyWidget::OnActorDestroyed(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	if (LockedDetailsActor == Actor)
	{
		LockedDetailsActor = nullptr;
		bDetailsLocked = false;
	}

	if (LastSelectedActor == Actor)
	{
		ResetSelection();
		return;
	}

	if (SelectedComponent && SelectedComponent->GetOwner() == Actor)
	{
		ResetSelection();
	}
}

void FEditorPropertyWidget::RenderDetailsLockBar(AActor* CurrentSelection, AActor* DisplayActor)
{
	ImGui::TextUnformatted("Inspector");
	ImGui::SameLine();

	const bool bCanLock = CurrentSelection != nullptr;
	ImGui::BeginDisabled(!bDetailsLocked && !bCanLock);
	if (ImGui::SmallButton(bDetailsLocked ? "Unlock" : "Lock"))
	{
		if (bDetailsLocked)
		{
			LockedDetailsActor = nullptr;
			bDetailsLocked = false;
			LastSelectedActor = nullptr;
			SelectedComponent = nullptr;
			bActorSelected = true;
		}
		else if (CurrentSelection)
		{
			LockedDetailsActor = CurrentSelection;
			bDetailsLocked = true;
			LastSelectedActor = nullptr;
			SelectedComponent = nullptr;
			bActorSelected = true;
		}
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (bDetailsLocked && DisplayActor)
	{
		FString LockedName = DisplayActor->GetFName().ToString();
		if (LockedName.empty()) LockedName = DisplayActor->GetClassName();
		ImGui::TextDisabled("Locked: %s", LockedName.c_str());
	}
	else
	{
		ImGui::TextDisabled("Unlocked");
	}

	DrawDetailsSeparator();
}

void FEditorPropertyWidget::UpdateSelectionState(AActor* PrimaryActor)
{
	UWorld* World = PrimaryActor->GetFocusedWorld();
	const FWorldContext* Ctx = EditorEngine->GetWorldContextFromWorld(World);

	if (PrimaryActor != LastSelectedActor)
	{
		SelectedComponent = nullptr;
		LastSelectedActor = PrimaryActor;
		bActorSelected = true;
	}

	if (!bDetailsLocked && Ctx->SelectionManager)
	{
		Ctx->SelectionManager->ValidateSelection();
		UActorComponent* ManagerComponent = Ctx->SelectionManager->GetSelectedComponent();
		if (IsLiveComponent(ManagerComponent) && ManagerComponent->GetOwner() == PrimaryActor)
		{
			SelectedComponent = ManagerComponent;
			bActorSelected = false;
		}
		else
		{
			SelectedComponent = nullptr;
			bActorSelected = true;
		}
	}
}

void FEditorPropertyWidget::SelectActorForDetails()
{
	const FWorldContext* Ctx = EditorEngine->GetFocusedWorldContext();
	bActorSelected = true;
	SelectedComponent = nullptr;
	if (Ctx->SelectionManager)
	{
		Ctx->SelectionManager->ClearComponentSelection();
	}
}

void FEditorPropertyWidget::SelectComponentForDetails(UActorComponent* Component)
{
	const FWorldContext* Ctx = EditorEngine->GetFocusedWorldContext();
	SelectedComponent = Component;
	bActorSelected = false;
	if (Ctx->SelectionManager)
	{
		Ctx->SelectionManager->SelectComponent(Component);
	}
}

void FEditorPropertyWidget::RenderActorHeaderRegion(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
	const int32 SelectionCount = static_cast<int32>(SelectedActors.size());

	if (SelectionCount > 1)
	{
		RenderMultiSelectionHeader(PrimaryActor, SelectedActors, SelectionCount);
	}
	else
	{
		RenderSingleSelectionHeader(PrimaryActor);
	}
}

void FEditorPropertyWidget::RenderMultiSelectionHeader(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors, int32 SelectionCount)
{
	ImGui::Text("Class: %s", PrimaryActor->GetClassName());

	FString PrimaryName = PrimaryActor->GetFName().ToString();
	if (PrimaryName.empty()) PrimaryName = PrimaryActor->GetClassName();

	const bool bWasActorSelected = bActorSelected;
	if (bWasActorSelected) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
	ImGui::Text("Name: %s (+%d)", PrimaryName.c_str(), SelectionCount - 1);
	if (bWasActorSelected) ImGui::PopStyleColor();

	if (ImGui::IsItemClicked())
	{
		SelectActorForDetails();
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		SelectActorForDetails();
		bOpenDetailsContextMenu = true;
	}
}

void FEditorPropertyWidget::RenderSingleSelectionHeader(AActor* PrimaryActor)
{
	const bool bWasActorSelected = bActorSelected;
	if (bWasActorSelected) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
	ImGui::Text("Actor: %s", PrimaryActor->GetFName().ToString().c_str());
	if (ImGui::IsItemClicked())
	{
		SelectActorForDetails();
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		SelectActorForDetails();
		bOpenDetailsContextMenu = true;
	}
	if (bWasActorSelected) ImGui::PopStyleColor();
}

void FEditorPropertyWidget::RenderDetailsContextMenu(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
	if (bOpenDetailsContextMenu)
	{
		ImGui::OpenPopup("##DetailsContextMenu");
		bOpenDetailsContextMenu = false;
	}

	if (!ImGui::BeginPopup("##DetailsContextMenu"))
	{
		return;
	}

	USceneComponent* AddAttachTarget = nullptr;
	if (!bActorSelected && PrimaryActor && SelectedComponent && SelectedComponent->GetOwner() == PrimaryActor)
	{
		AddAttachTarget = Cast<USceneComponent>(SelectedComponent);
	}

	if (AddAttachTarget && ImGui::BeginMenu("Add Component"))
	{
		if (UClass* ComponentClass = FComponentMenuRegistry::DrawSpawnableComponentClassMenu())
		{
			if (EditorEngine)
			{
				EditorEngine->GetUndoSystem().CaptureSnapshot("Add Component");
			}
			if (UActorComponent* NewComp = PrimaryActor->AddComponentByClass(ComponentClass))
			{
				InitializeSpawnedComponentDefaults(NewComp);
				AttachAndSelectNewComponent(PrimaryActor, NewComp, AddAttachTarget);
				if (EditorEngine)
				{
					EditorEngine->GetSceneService().MarkDirty();
				}
			}
		}
		ImGui::EndMenu();
	}

	DrawDetailsSeparator();
	if (bActorSelected)
	{
		ImGui::BeginDisabled(SelectedActors.empty());
		if (ImGui::MenuItem(SelectedActors.size() > 1 ? "Delete Actors" : "Delete Actor", "Del"))
		{
			TArray<AActor*> ActorsToDelete = SelectedActors;
			if (EditorEngine && EditorEngine->DeleteActors(ActorsToDelete) > 0)
			{
				SelectedComponent = nullptr;
				LastSelectedActor = nullptr;
				bActorSelected = true;
			}
		}
		ImGui::EndDisabled();
	}
	else
	{
		const bool bCanDeleteComponent = CanDeleteComponent(PrimaryActor, SelectedComponent);
		ImGui::BeginDisabled(!bCanDeleteComponent);
		if (ImGui::MenuItem("Delete Component", "Del"))
		{
			DeleteSelectedComponent(PrimaryActor);
		}
		ImGui::EndDisabled();
		if (!bCanDeleteComponent && PrimaryActor && SelectedComponent == PrimaryActor->GetRootComponent())
		{
			ImGui::TextDisabled("Root component cannot be deleted.");
		}
	}

	ImGui::EndPopup();
}

void FEditorPropertyWidget::RenderComponentTree(AActor* Actor)
{
	if (!IsLiveActor(Actor))
	{
		ImGui::TextDisabled("Selected actor is no longer available.");
		return;
	}

	DrawDetailsSectionLabel("Components");
	DrawDetailsSeparator();

	float TreeHeight = std::max(64.0f, ImGui::GetContentRegionAvail().y * 0.2f);

	// BeginChild를 호출하여 내부 스크롤이 가능한 Child Window를 생성합니다.
	ImGui::BeginChild("##ComponentTreeChild", ImVec2(0, TreeHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

	USceneComponent* Root = Actor->GetRootComponent();
	FString ActorName = Actor->GetFName().ToString();
	if (ActorName.empty()) ActorName = Actor->GetClassName();

	ImGuiTreeNodeFlags ActorFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
	if (bActorSelected) ActorFlags |= ImGuiTreeNodeFlags_Selected;

	const bool bActorNodeOpen = ImGui::TreeNodeEx(Actor, ActorFlags, "%s (Instance)", ActorName.c_str());
	if (ImGui::IsItemClicked())
	{
		SelectActorForDetails();
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		SelectActorForDetails();
		bOpenDetailsContextMenu = true;
	}

	if (bActorNodeOpen)
	{
		if (Root)
		{
			RenderSceneComponentNode(Actor, Root);
		}

		// Non-scene ActorComponents 및 MovementComponent들 하단 출력
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			// SceneComponent는 위의 트리 렌더링에서 처리되었으므로 패스
			if (!IsLiveComponent(Comp) || Comp->IsA<USceneComponent>())
				continue;

			ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (!bActorSelected && SelectedComponent == Comp)
				Flags |= ImGuiTreeNodeFlags_Selected;

			// MovementComponent 일 때와 일반 컴포넌트 일 때의 출력 형식 분리
			if (UMovementComponent* MoveComp = Cast<UMovementComponent>(Comp))
			{
				FString MoveName = GetMovementComponentDisplayName(MoveComp);
				ImGui::TreeNodeEx(Comp, Flags, "%s", MoveName.c_str());

				// --- DRAG SOURCE (MovementComponent) ---
				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload("DND_MOVE_COMP", &Comp, sizeof(UActorComponent*));
					ImGui::Text("Moving %s", MoveName.c_str());
					ImGui::EndDragDropSource();
				}
			}
			else
			{
				FString Name = Comp->GetFName().ToString();
				ImGui::TreeNodeEx(Comp, Flags, "%s", Name.c_str());
			}

			if (ImGui::IsItemClicked())
			{
				SelectComponentForDetails(Comp);
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
			{
				SelectComponentForDetails(Comp);
				bOpenDetailsContextMenu = true;
			}

		}

		ImGui::TreePop();
	}

	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)
		&& !ImGui::IsAnyItemHovered()
		&& ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		SelectActorForDetails();
		bOpenDetailsContextMenu = true;
	}

	ImGui::EndChild();
}

void FEditorPropertyWidget::RenderSceneComponentNode(AActor* Actor, USceneComponent* Comp)
{
	if (!IsLiveActor(Actor) || !IsLiveComponent(Comp)) return;

	FString Name = Comp->GetFName().ToString();
	if (Name.empty()) Name = Comp->GetClassName();

	const auto& Children = Comp->GetChildren();

	bool bHasChildren = !Children.empty(); // 자식 무브먼트 체크 제거

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
	if (!bHasChildren) Flags |= ImGuiTreeNodeFlags_Leaf;
	if (!bActorSelected && SelectedComponent == Comp) Flags |= ImGuiTreeNodeFlags_Selected;

	bool bIsRoot = (Comp->GetParent() == nullptr);

	bool bOpen = ImGui::TreeNodeEx(
		Comp, Flags, "%s%s",
		Name.c_str(),
		bIsRoot ? " (Root)" : ""
	);

	// --- DRAG SOURCE (SceneComponent) ---
	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("DND_SCENE_COMP", &Comp, sizeof(USceneComponent*));
		ImGui::Text("Dragging %s", Name.c_str());
		ImGui::EndDragDropSource();
	}

	// --- DROP TARGET ---
	if (ImGui::BeginDragDropTarget())
	{
		// 1. SceneComponent를 SceneComponent에 드롭 (부착)
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("DND_SCENE_COMP"))
		{
			USceneComponent* DraggedComp = *(USceneComponent**)Payload->Data;
			// 자기 자신이나 자신의 조상에게 부착하는 것을 방지
			bool bIsAncestor = false;
			for (USceneComponent* P = Comp; P; P = P->GetParent())
			{
				if (P == DraggedComp) { bIsAncestor = true; break; }
			}

			if (DraggedComp && DraggedComp != Comp && !bIsAncestor)
			{
				if (EditorEngine)
				{
					EditorEngine->GetUndoSystem().CaptureSnapshot("Attach Component");
				}
				DraggedComp->AttachToComponent(Comp);
			}
		}
		// 2. MovementComponent를 SceneComponent에 드롭 (UpdatedComponent 설정)
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("DND_MOVE_COMP"))
		{
			UMovementComponent* DraggedMoveComp = *(UMovementComponent**)Payload->Data;
			if (DraggedMoveComp)
			{
				if (EditorEngine)
				{
					EditorEngine->GetUndoSystem().CaptureSnapshot("Set Updated Component");
				}
				DraggedMoveComp->SetUpdatedComponent(Comp);
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (ImGui::IsItemClicked())
	{
		SelectComponentForDetails(Comp);
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		SelectComponentForDetails(Comp);
		bOpenDetailsContextMenu = true;
	}

	if (bOpen)
	{
		for (USceneComponent* Child : Children)
		{
			if (IsLiveComponent(Child))
			{
				RenderSceneComponentNode(Actor, Child);
			}
		}

		ImGui::TreePop();
	}
}

bool FEditorPropertyWidget::CanDeleteComponent(AActor* Owner, UActorComponent* Component) const
{
	if (!Owner || !Component)
	{
		return false;
	}

	if (Component == Owner->GetRootComponent())
	{
		return false;
	}

	if (USceneComponent* SceneComp = Cast<USceneComponent>(Component))
	{
		for (UActorComponent* ActorComp : Owner->GetComponents())
		{
			if (UMovementComponent* MoveComp = Cast<UMovementComponent>(ActorComp))
			{
				if (MoveComp->GetUpdatedComponent() == SceneComp)
				{
					return false;
				}
			}
		}
	}

	return true;
}

void FEditorPropertyWidget::DeleteSelectedComponent(AActor* Owner)
{
	if (!CanDeleteComponent(Owner, SelectedComponent))
	{
		return;
	}

	UWorld* World = Owner->GetFocusedWorld();
	const FWorldContext* Ctx = EditorEngine->GetWorldContextFromWorld(World);

	UActorComponent* ComponentToDelete = SelectedComponent;
	if (EditorEngine)
	{
		EditorEngine->GetUndoSystem().CaptureSnapshot("Delete Component");
	}
	SelectedComponent = nullptr;
	bActorSelected = true;
	if (Ctx->SelectionManager)
	{
		Ctx->SelectionManager->OnComponentDestroyed(ComponentToDelete);
	}
	Owner->RemoveComponent(ComponentToDelete);
	if (EditorEngine)
	{
		EditorEngine->GetSceneService().MarkDirty();
	}
}

void FEditorPropertyWidget::RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
	if (bActorSelected)
	{
		RenderActorProperties(PrimaryActor, SelectedActors);
	}
	else if (SelectedComponent)
	{
		RenderComponentProperties();
	}
	else
	{
		ImGui::TextDisabled("Select an actor or component to view details.");
	}
}

void FEditorPropertyWidget::RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
	ImGui::Text("Actor: %s", PrimaryActor->GetClassName());
	RenderEditableName("Name##Actor", PrimaryActor, &bFocusActorNameNextFrame); // 편집 가능한 UI

	if (PrimaryActor->GetRootComponent())
	{
		DrawDetailsSeparator();
		DrawDetailsSectionLabel("Transform");
		ImGui::Spacing();

		// FVector(위치, 회전, 크기)를 읽어서 Properties를 그려 주는 단순한 친구입니다.
		auto DrawTransformField = [&](const char* Label, FVector CurrentValue, auto ApplyFunc)
		{
			float Arr[3] = { CurrentValue.X, CurrentValue.Y, CurrentValue.Z };
			const bool bEdited = ImGui::DragFloat3(Label, Arr, 0.1f);
			if (ImGui::IsItemActivated() && EditorEngine)
			{
				EditorEngine->GetUndoSystem().CaptureSnapshot("Transform Actors");
			}
			if (bEdited)
			{
				FVector Delta = FVector(Arr[0], Arr[1], Arr[2]) - CurrentValue;
				for (AActor* Actor : SelectedActors)
				{
					if (Actor) ApplyFunc(Actor, Delta);
				}

				UWorld* World = PrimaryActor->GetFocusedWorld();
				const FWorldContext* Ctx = EditorEngine->GetWorldContextFromWorld(World);
				Ctx->SelectionManager->GetGizmo()->UpdateGizmoTransform();
			}
		};

		// Location, Rotation, Scale을 한 번에 그려줍니다.
		DrawTransformField("Location", PrimaryActor->GetActorLocation(), [](AActor* A, FVector D) { A->AddActorWorldOffset(D); });
		DrawTransformField("Rotation", PrimaryActor->GetActorRotation(), [](AActor* A, FVector D) { A->SetActorRotation(A->GetActorRotation() + D); });
		DrawTransformField("Scale",    PrimaryActor->GetActorScale(),    [](AActor* A, FVector D) { A->SetActorScale(A->GetActorScale() + D); });
	}

	DrawDetailsSeparator();

	TArray<const FProperty*> ReflectedProperties;
	CollectEditableReflectedProperties(PrimaryActor, ReflectedProperties);
	if (!ReflectedProperties.empty())
	{
		DrawDetailsSectionLabel("Properties");
		ImGui::Spacing();
		RenderReflectionProperties(PrimaryActor);
	}

	RenderDebugDetails(PrimaryActor, PrimaryActor, SelectedActors);
}

void FEditorPropertyWidget::RenderActorTags(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
	if (!PrimaryActor)
	{
		return;
	}

	DrawDetailsSeparator();
	DrawDetailsSectionLabel("Actor Tags");
	if (SelectedActors.size() > 1)
	{
		ImGui::TextDisabled("Tag edits apply to selected actors.");
	}

	const TArray<FString> Tags = PrimaryActor->GetTags();
	if (Tags.empty())
	{
		ImGui::TextDisabled("No tags.");
	}
	else
	{
		for (int32 TagIndex = 0; TagIndex < static_cast<int32>(Tags.size()); ++TagIndex)
		{
			const FString& Tag = Tags[TagIndex];
			ImGui::PushID(TagIndex);
			ImGui::AlignTextToFramePadding();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.76f, 0.84f, 1.0f));
			ImGui::TextUnformatted(Tag.c_str());
			ImGui::PopStyleColor();
			ImGui::SameLine();
			if (ImGui::SmallButton("Remove"))
			{
				bool bChanged = false;
				for (AActor* Actor : SelectedActors)
				{
					if (Actor && Actor->HasTag(Tag))
					{
						if (!bChanged && EditorEngine)
						{
							EditorEngine->GetUndoSystem().CaptureSnapshot("Remove Actor Tag");
						}
						Actor->RemoveTag(Tag);
						bChanged = true;
					}
				}
				if (bChanged && EditorEngine)
				{
					EditorEngine->GetSceneService().MarkDirty();
				}
			}
			ImGui::PopID();
		}
	}

	ImGui::Spacing();
	ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - 58.0f));
	const bool bAddByEnter = ImGui::InputTextWithHint(
		"##NewActorTag",
		"New tag",
		NewActorTagBuffer,
		IM_ARRAYSIZE(NewActorTagBuffer),
		ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::SameLine();
	const bool bAddByButton = ImGui::Button("Add", ImVec2(52.0f, 0.0f));

	if ((bAddByEnter || bAddByButton) && NewActorTagBuffer[0] != '\0')
	{
		const FString NewTag = NewActorTagBuffer;
		bool bChanged = false;
		for (AActor* Actor : SelectedActors)
		{
			if (Actor && !Actor->HasTag(NewTag))
			{
				if (!bChanged && EditorEngine)
				{
					EditorEngine->GetUndoSystem().CaptureSnapshot("Add Actor Tag");
				}
				Actor->AddTag(NewTag);
				bChanged = true;
			}
		}
		if (bChanged)
		{
			NewActorTagBuffer[0] = '\0';
			if (EditorEngine)
			{
				EditorEngine->GetSceneService().MarkDirty();
			}
		}
	}
}

void FEditorPropertyWidget::RenderComponentTags(UActorComponent* Component)
{
	if (!Component)
	{
		return;
	}

	const TArray<FString> Tags = Component->GetTags();
	if (Tags.empty())
	{
		ImGui::TextDisabled("No tags.");
	}
	else
	{
		for (int32 TagIndex = 0; TagIndex < static_cast<int32>(Tags.size()); ++TagIndex)
		{
			const FString& Tag = Tags[TagIndex];
			ImGui::PushID(TagIndex);
			ImGui::AlignTextToFramePadding();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.76f, 0.84f, 1.0f));
			ImGui::TextUnformatted(Tag.c_str());
			ImGui::PopStyleColor();
			ImGui::SameLine();
			if (ImGui::SmallButton("Remove"))
			{
				if (Component->HasTag(Tag))
				{
					if (EditorEngine)
					{
						EditorEngine->GetUndoSystem().CaptureSnapshot("Remove Component Tag");
					}
					Component->RemoveTag(Tag);
					if (EditorEngine)
					{
						EditorEngine->GetSceneService().MarkDirty();
					}
				}
			}
			ImGui::PopID();
		}
	}

	ImGui::Spacing();
	ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - 58.0f));
	const bool bAddByEnter = ImGui::InputTextWithHint(
		"##NewComponentTag",
		"New tag",
		NewComponentTagBuffer,
		IM_ARRAYSIZE(NewComponentTagBuffer),
		ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::SameLine();
	const bool bAddByButton = ImGui::Button("Add", ImVec2(52.0f, 0.0f));

	if ((bAddByEnter || bAddByButton) && NewComponentTagBuffer[0] != '\0')
	{
		const FString NewTag = NewComponentTagBuffer;
		if (!Component->HasTag(NewTag))
		{
			if (EditorEngine)
			{
				EditorEngine->GetUndoSystem().CaptureSnapshot("Add Component Tag");
			}
			Component->AddTag(NewTag);
			NewComponentTagBuffer[0] = '\0';
			if (EditorEngine)
			{
				EditorEngine->GetSceneService().MarkDirty();
			}
		}
	}
}

void FEditorPropertyWidget::RenderComponentProperties()
{
	bDetailsPerfTraceFrame = false;
	if (SelectedComponent != LastDetailsPerfComponent)
	{
		LastDetailsPerfComponent = SelectedComponent;
		bDetailsPerfTraceFrame =
			SelectedComponent &&
			(Cast<UStaticMeshComponent>(SelectedComponent) || Cast<USkeletalMeshComponent>(SelectedComponent));
	}

	const FDetailsPerfClock::time_point TotalStart = bDetailsPerfTraceFrame ? FDetailsPerfClock::now() : FDetailsPerfClock::time_point{};

	ImGui::Text("Component: %s", SelectedComponent->GetClassName());
	RenderEditableName("Name##Component", SelectedComponent, &bFocusComponentNameNextFrame); // 편집 가능한 UI

	DrawDetailsSeparator();

	TArray<const FProperty*> ReflectedProperties;
	const FDetailsPerfClock::time_point PropertiesStart = bDetailsPerfTraceFrame ? FDetailsPerfClock::now() : FDetailsPerfClock::time_point{};
	CollectEditableReflectedProperties(SelectedComponent, ReflectedProperties);
	const FDetailsPerfClock::time_point PropertiesEnd = bDetailsPerfTraceFrame ? FDetailsPerfClock::now() : FDetailsPerfClock::time_point{};

	AActor* Owner = SelectedComponent->GetOwner();
	double PropertyWidgetMs = 0.0;
	int32 RenderedPropertyCount = 0;

	const bool bUseSkeletalMeshSection = Cast<USkeletalMeshComponent>(SelectedComponent) != nullptr;
	bool bRenderedSkeletalMeshSection = false;
	if (bUseSkeletalMeshSection)
	{
		for (const FProperty* Property : ReflectedProperties)
		{
			if (!Property || !Property->Name || std::strcmp(Property->Name, "Tags") == 0 || !IsSkeletalMeshSectionProperty(Property))
			{
				continue;
			}

			if (!bRenderedSkeletalMeshSection)
			{
				DrawDetailsSectionLabel("Skeletal Mesh");
				ImGui::Spacing();
				bRenderedSkeletalMeshSection = true;
			}

			const FDetailsPerfClock::time_point PropStart = bDetailsPerfTraceFrame ? FDetailsPerfClock::now() : FDetailsPerfClock::time_point{};
			RenderReflectionProperty(FPropertyHandle{ SelectedComponent, Property });
			++RenderedPropertyCount;
			if (bDetailsPerfTraceFrame)
			{
				PropertyWidgetMs += DetailsPerfMs(PropStart, FDetailsPerfClock::now());
			}
		}
	}

	if (bRenderedSkeletalMeshSection)
	{
		DrawDetailsSeparator();
	}
	DrawDetailsSectionLabel("Component Tags");
	ImGui::Spacing();
	RenderComponentTags(SelectedComponent);

	bool bRenderedPropertiesSection = false;
	for (const FProperty* Property : ReflectedProperties)
	{
		if (!Property || !Property->Name || strcmp(Property->Name, "Tags") == 0)
		{
			continue;
		}

		if (bUseSkeletalMeshSection && IsSkeletalMeshSectionProperty(Property))
		{
			continue;
		}

		if (!bRenderedPropertiesSection)
		{
			DrawDetailsSeparator();
			DrawDetailsSectionLabel("Properties");
			ImGui::Spacing();
			bRenderedPropertiesSection = true;
		}

		const FDetailsPerfClock::time_point PropStart = bDetailsPerfTraceFrame ? FDetailsPerfClock::now() : FDetailsPerfClock::time_point{};
		RenderReflectionProperty(FPropertyHandle{ SelectedComponent, Property });
		++RenderedPropertyCount;
		if (bDetailsPerfTraceFrame)
		{
			PropertyWidgetMs += DetailsPerfMs(PropStart, FDetailsPerfClock::now());
		}
	}

	double SkeletalDebugMs = 0.0;
	RenderDebugDetails(SelectedComponent, Owner, TArray<AActor*>{ Owner });

	// 프로퍼티 직접 편집 후 월드 행렬 갱신
	if (SelectedComponent->IsA<USceneComponent>())
	{
		UWorld* World = Owner->GetFocusedWorld();
		const FWorldContext* Ctx = EditorEngine->GetWorldContextFromWorld(World);
		static_cast<USceneComponent*>(SelectedComponent)->MarkTransformDirty();
		Ctx->SelectionManager->GetGizmo()->UpdateGizmoTransform();
	}

	if (bDetailsPerfTraceFrame)
	{
		const double CollectPropertiesMs = DetailsPerfMs(PropertiesStart, PropertiesEnd);
		const double TotalMs = DetailsPerfMs(TotalStart, FDetailsPerfClock::now());
		UE_LOG(
			"[DetailsPerf] Component=%s Type=%s Total=%.2fms CollectProperties=%.2fms PropertyWidgets=%.2fms SkeletalDebug=%.2fms Props=%d",
			SelectedComponent ? SelectedComponent->GetFName().ToString().c_str() : "<None>",
			SelectedComponent ? SelectedComponent->GetClassName() : "<None>",
			TotalMs,
			CollectPropertiesMs,
			PropertyWidgetMs,
			SkeletalDebugMs,
			RenderedPropertyCount);
	}
}

void FEditorPropertyWidget::RenderReflectionProperties(UObject* Object)
{
	TArray<const FProperty*> Properties;
	CollectEditableReflectedProperties(Object, Properties);
	for (const FProperty* Property : Properties)
	{
		if (!Property)
		{
			continue;
		}
		RenderReflectionProperty(FPropertyHandle{ Object, Property });
	}
}

void FEditorPropertyWidget::RenderReflectionProperty(const FPropertyHandle& Handle)
{
	if (!Handle.IsValid() || !Handle.IsEditable())
	{
		return;
	}

	RenderPropertyWidget(Handle);
}

void FEditorPropertyWidget::RenderDebugDetails(UObject* Object, AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
	(void)PrimaryActor;
	if (!Object)
	{
		return;
	}

	FDebugDetailsBuilder Builder;
	Object->BuildDebugDetails(Builder);

	if (AActor* Actor = Cast<AActor>(Object))
	{
		Builder.AddTagEditor("Actor Tags", [this, Actor, &SelectedActors]()
		{
			RenderActorTags(Actor, SelectedActors);
		});

		if (UBillboardComponent* BillboardComp = Cast<UBillboardComponent>(Actor->GetRootComponent()))
		{
			if (!Cast<USubUVComponent>(Actor->GetRootComponent()))
			{
				Builder.AddCustom([this, BillboardComp, &SelectedActors]()
				{
					DrawDetailsSeparator();
					DrawDetailsSectionLabel("Sprite Texture");

					const TArray<FString>& TextureList = EditorEngine
						? EditorEngine->GetAssetService().GetTextureAssetPaths()
						: EmptyAssetNames();
					const FString CurrentName = BillboardComp->GetTextureName();

					if (ImGui::BeginCombo("##SpriteTexture", CurrentName.empty() ? "None" : CurrentName.c_str()))
					{
						for (const FString& TexPath : TextureList)
						{
							const bool bSelected = TexPath == CurrentName;
							if (ImGui::Selectable(TexPath.c_str(), bSelected))
							{
								if (EditorEngine)
								{
									EditorEngine->GetUndoSystem().CaptureSnapshot("Edit Billboard");
								}
								for (AActor* SelectedActor : SelectedActors)
								{
									if (UBillboardComponent* Comp = SelectedActor
										? Cast<UBillboardComponent>(SelectedActor->GetRootComponent())
										: nullptr)
									{
										Comp->SetTextureName(TexPath);
									}
								}
							}
							if (bSelected)
							{
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}
				});
			}
		}

	}
	else if (UActorComponent* Component = Cast<UActorComponent>(Object))
	{
		if (USkeletalMeshComponent* SkeletalComp = Cast<USkeletalMeshComponent>(Component))
		{
			Builder.AddCustom([this, SkeletalComp]()
			{
				if (UAnimInstance* AnimInstance = SkeletalComp->GetAnimInstance())
				{
					TArray<const FProperty*> AnimProperties;
					CollectEditableReflectedProperties(AnimInstance, AnimProperties);
					if (!AnimProperties.empty())
					{
						DrawDetailsSeparator();
						DrawDetailsSectionLabel("Anim Instance");
						for (const FProperty* Property : AnimProperties)
						{
							RenderReflectionProperty(FPropertyHandle{ AnimInstance, Property });
						}
					}
				}
				RenderSkeletalStateMachinePreview(SkeletalComp);
				RenderSkeletalBonePoseDebug(SkeletalComp);
			});
		}

		if (UInterpToMovementComponent* InterpComp = Cast<UInterpToMovementComponent>(Component))
		{
			Builder.AddCustom([this, InterpComp]()
			{
				RenderInterpControlPoints(InterpComp);
			});
		}

		if (UActorSequenceComponent* SequenceComp = Cast<UActorSequenceComponent>(Component))
		{
			Builder.AddCustom([this, SequenceComp]()
			{
				ActorSequenceDetails.Render(SequenceComp, LastDeltaTime);
			});
		}

		if (ULightComponent* LightComp = Cast<ULightComponent>(Component))
		{
			Builder.AddButton("Override camera with light's perspective", [LightComp]()
			{
				UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;
				FViewportCamera* Camera = World ? World->GetActiveCamera() : nullptr;
				if (Camera)
				{
					Camera->SetLocation(LightComp->GetWorldLocation());
					Camera->SetRotation(LightComp->GetRelativeQuat());
				}
			});
		}
		else if (UScriptComponent* ScriptComp = Cast<UScriptComponent>(Component))
		{
			Builder.AddCustom([this, ScriptComp]()
			{
				FScriptManager& ScriptMgr = FScriptManager::Get();
				DrawDetailsSeparator();
				DrawDetailsSectionLabel("Script Actions");

				if (ImGui::Button("Create Script"))
				{
					FString ScriptPath = ScriptComp->GetScriptName();
					if (ScriptPath.empty() || IsBlankString(ScriptPath))
					{
						if (EditorEngine)
						{
							EditorEngine->GetNotificationService().Warning("Script name is empty");
						}
					}
					else
					{
						FString SelectedScriptPath;
						if (!PromptCreateScriptAs(EditorEngine, ScriptPath, SelectedScriptPath))
						{
							return;
						}

						if (!ScriptMgr.CreateScript(SelectedScriptPath))
						{
							if (EditorEngine)
							{
								EditorEngine->GetNotificationService().Error("Script create failed");
							}
							return;
						}

						ScriptComp->SetScriptName(MakeScriptReferenceFromPath(SelectedScriptPath));
						ScriptComp->ReloadLuaProperties();
						if (EditorEngine)
						{
							EditorEngine->GetNotificationService().Info("Script created");
						}
					}
				}

				if (ImGui::Button("Edit Script"))
				{
					FString ScriptPath = ScriptComp->GetScriptName();
					if (ScriptPath.empty() || IsBlankString(ScriptPath))
					{
						if (EditorEngine)
						{
							EditorEngine->GetNotificationService().Warning("No script selected");
						}
					}
					else if (!ScriptMgr.EditScript(ScriptPath) && EditorEngine)
					{
						EditorEngine->GetNotificationService().Warning("Script file not found");
					}
				}
			});
		}
	}

	if (Builder.IsEmpty())
	{
		return;
	}

	for (const FDebugDetailsItem& Item : Builder.GetItems())
	{
		RenderDebugDetailsItem(Item);
	}
}

void FEditorPropertyWidget::RenderDebugDetailsItem(const FDebugDetailsItem& Item)
{
	switch (Item.Type)
	{
	case EDebugDetailsItemType::Text:
		DrawDetailsSeparator();
		DrawDetailsSectionLabel(Item.Label.c_str());
		ImGui::TextUnformatted(Item.Value.c_str());
		break;
	case EDebugDetailsItemType::Button:
		DrawDetailsSeparator();
		if (ImGui::Button(Item.Label.c_str()) && Item.Callback)
		{
			Item.Callback();
		}
		break;
	case EDebugDetailsItemType::SRVPreview:
		if (Item.SRVPreview.SRV)
		{
			const FSRVDisplayInfo& Info = Item.SRVPreview.DisplayInfo;
			DrawDetailsSeparator();
			DrawDetailsSectionLabel(Item.Label.c_str());
			ImGui::Image(
				Item.SRVPreview.SRV,
				ImVec2(Info.ImageWidth, Info.ImageHeight),
				ImVec2(Info.UV0X, Info.UV0Y),
				ImVec2(Info.UV1X, Info.UV1Y));
		}
		break;
	case EDebugDetailsItemType::CubeSRVPreview:
	{
		static const char* FaceLabels[6] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };
		const FSRVDisplayInfo& Info = Item.CubeSRVPreview.DisplayInfo;
		bool bHasAnyFace = false;
		for (ID3D11ShaderResourceView* FaceSRV : Item.CubeSRVPreview.FaceSRVs)
		{
			bHasAnyFace = bHasAnyFace || FaceSRV != nullptr;
		}
		if (!bHasAnyFace)
		{
			break;
		}

		DrawDetailsSeparator();
		DrawDetailsSectionLabel(Item.Label.c_str());
		for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
		{
			ID3D11ShaderResourceView* FaceSRV = Item.CubeSRVPreview.FaceSRVs[FaceIndex];
			if (!FaceSRV)
			{
				continue;
			}

			ImGui::BeginGroup();
			ImGui::TextUnformatted(FaceLabels[FaceIndex]);
			ImGui::Image(
				FaceSRV,
				ImVec2(Info.ImageWidth, Info.ImageHeight),
				ImVec2(Info.UV0X, Info.UV0Y),
				ImVec2(Info.UV1X, Info.UV1Y));
			ImGui::EndGroup();

			if ((FaceIndex % 3) != 2)
			{
				ImGui::SameLine();
			}
		}
		break;
	}
	case EDebugDetailsItemType::TagEditor:
	case EDebugDetailsItemType::Custom:
		if (Item.Callback)
		{
			Item.Callback();
		}
		break;
	default:
		break;
	}
}

bool FEditorPropertyWidget::RenderObjectPtrWidget(const FProperty& Property, void* ValuePtr, UObject* NotifyTarget, const char* Label, int32 ArrayIndex)
{
	if (!Property.ObjectPtrOps || !ValuePtr)
	{
		return false;
	}

	UObject* CurrentObject = Property.ObjectPtrOps->GetObject(ValuePtr);
	const bool bMaterialAsset = Property.ReferenceKind == EObjectReferenceKind::Asset
		&& Property.ObjectClass
		&& Property.ObjectClass->IsChildOf(UMaterialInterface::StaticClass());

	if (bMaterialAsset && EditorEngine)
	{
		FEditorAssetService& AssetService = EditorEngine->GetAssetService();
		const TArray<FString>& MaterialNames = AssetService.GetMaterialInterfaceNames();
		UMaterialInterface* CurrentMaterial = Cast<UMaterialInterface>(CurrentObject);
		const FString CurrentIdentifier = CurrentMaterial
			? (CurrentMaterial->GetFilePath().empty() ? CurrentMaterial->GetName() : FPaths::Normalize(CurrentMaterial->GetFilePath()))
			: FString();
		const FString CurrentLabel = CurrentIdentifier.empty() ? FString("None") : CurrentIdentifier;
		bool bChanged = false;

		if (ImGui::BeginCombo(Label, CurrentLabel.c_str()))
		{
			if (ImGui::Selectable("None", CurrentMaterial == nullptr))
			{
				Property.ObjectPtrOps->SetObject(ValuePtr, nullptr);
				bChanged = true;
			}

			for (int32 MaterialIndex = 0; MaterialIndex < static_cast<int32>(MaterialNames.size()); ++MaterialIndex)
			{
				ImGui::PushID(MaterialIndex);
				const FString& MaterialLabel = MaterialNames[MaterialIndex].empty()
					? FString("<Unnamed Material>")
					: MaterialNames[MaterialIndex];
				const bool bSelected = CurrentIdentifier == MaterialLabel;
				if (ImGui::Selectable(MaterialLabel.c_str(), bSelected))
				{
					if (UMaterialInterface* Candidate = AssetService.ResolveMaterialInterfaceByIndex(MaterialIndex))
					{
						Property.ObjectPtrOps->SetObject(ValuePtr, Candidate);
						bChanged = true;
					}
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
				if (ImGui::IsItemHovered())
				{
					if (UMaterialInterface* Candidate = AssetService.ResolveMaterialInterfaceByIndex(MaterialIndex))
					{
						RenderMaterialPreviewTooltip(Candidate);
					}
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered() && CurrentMaterial)
		{
			RenderMaterialPreviewTooltip(CurrentMaterial);
		}

		if (ArrayIndex >= 0)
		{
			ImGui::SameLine();
			if (ImGui::Button("Edit"))
			{
				if (UPrimitiveComponent* PrimitiveComp = Cast<UPrimitiveComponent>(SelectedComponent))
				{
					EditorEngine->GetMainPanel().OpenMaterialSlot(PrimitiveComp, ArrayIndex);
				}
			}
		}

		return bChanged;
	}

	AActor* Owner = nullptr;
	if (AActor* Actor = Cast<AActor>(NotifyTarget))
	{
		Owner = Actor;
	}
	else if (UActorComponent* Component = Cast<UActorComponent>(NotifyTarget))
	{
		Owner = Component->GetOwner();
	}
	else if (SelectedComponent)
	{
		Owner = SelectedComponent->GetOwner();
	}

	TArray<UObject*> Choices;
	Choices.push_back(nullptr);
	if (Owner)
	{
		for (UActorComponent* Component : Owner->GetComponents())
		{
			if (!Component)
			{
				continue;
			}
			if (!Property.ObjectClass || Component->IsA(Property.ObjectClass))
			{
				Choices.push_back(Component);
			}
		}
	}

	auto GetLabel = [&](UObject* Object) -> FString
	{
		if (!Object)
		{
			return "None";
		}
		FString Name = Object->GetFName().ToString();
		if (Name.empty())
		{
			Name = Object->GetClassName();
		}
		if (Owner && Object == Owner->GetRootComponent())
		{
			return "[Root] " + Name;
		}
		return Name;
	};

	bool bChanged = false;
	if (ImGui::BeginCombo(Label, GetLabel(CurrentObject).c_str()))
	{
		for (UObject* Candidate : Choices)
		{
			const bool bSelected = Candidate == CurrentObject;
			const FString CandidateLabel = GetLabel(Candidate);
			char SelectableId[128];
			snprintf(SelectableId, sizeof(SelectableId), "%s##%p", CandidateLabel.c_str(), static_cast<void*>(Candidate));
			if (ImGui::Selectable(SelectableId, bSelected))
			{
				Property.ObjectPtrOps->SetObject(ValuePtr, Candidate);
				bChanged = true;
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	return bChanged;
}

bool FEditorPropertyWidget::RenderSoftObjectPtrWidget(const FProperty& Property, void* ValuePtr, const char* Label)
{
	if (!Property.SoftObjectOps || !ValuePtr)
	{
		return false;
	}

	bool bChanged = false;
	FString Current = Property.SoftObjectOps->GetPath(ValuePtr);
	TArray<FString> LocalOptions;
	const TArray<FString>* Options = nullptr;
	if (EditorEngine && Property.ObjectClass)
	{
		if (Property.ObjectClass->IsChildOf(UStaticMesh::StaticClass()))
		{
			Options = &EditorEngine->GetAssetService().GetStaticMeshAssetPaths();
		}
		else if (Property.ObjectClass->IsChildOf(USkeletalMesh::StaticClass()))
		{
			Options = &EditorEngine->GetAssetService().GetSkeletalMeshAssetPaths();
		}
		else if (Property.ObjectClass->IsChildOf(UMaterialInterface::StaticClass()))
		{
			Options = &EditorEngine->GetAssetService().GetMaterialInterfaceNames();
		}
		else if (Property.ObjectClass->IsChildOf(UAnimationAsset::StaticClass()))
		{
			LocalOptions = FResourceManager::Get().GetAnimSequencePaths();
			Options = &LocalOptions;
		}
	}

	if (Options && !Options->empty())
	{
		if (ImGui::BeginCombo(Label, Current.empty() ? "<None>" : Current.c_str()))
		{
			if (ImGui::Selectable("<None>", Current.empty()))
			{
				Property.SoftObjectOps->SetPath(ValuePtr, FString());
				bChanged = true;
			}
			for (const FString& Path : *Options)
			{
				const bool bSelected = Current == Path;
				if (ImGui::Selectable(Path.c_str(), bSelected))
				{
					Property.SoftObjectOps->SetPath(ValuePtr, Path);
					bChanged = true;
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}
	else
	{
		char Buf[512];
		strncpy_s(Buf, sizeof(Buf), Current.c_str(), _TRUNCATE);
		if (ImGui::InputText(Label, Buf, sizeof(Buf)))
		{
			Property.SoftObjectOps->SetPath(ValuePtr, Buf);
			bChanged = true;
		}
	}

	return bChanged;
}

bool FEditorPropertyWidget::RenderArrayPropertyWidget(const FProperty& Property, void* ValuePtr, UObject* NotifyTarget)
{
	if (!Property.ArrayOps || !Property.InnerProperty || !ValuePtr)
	{
		return false;
	}

	bool bChanged = false;
	int32 ToRemove = -1;
	DrawDetailsSeparator();
	DrawDetailsSectionLabel(GetPropertyDisplayName(Property));
	ImGui::PushID(Property.Name);

	const int32 Count = Property.ArrayOps->Num(ValuePtr);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		ImGui::PushID(Index);
		void* ElementPtr = Property.ArrayOps->GetElementPtr(ValuePtr, Index);
		char ItemLabel[32];
		snprintf(ItemLabel, sizeof(ItemLabel), "[%d]", Index);

		ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x - UIConstants::XButtonSize - 8.0f));
		if (RenderPropertyValueWidget(*Property.InnerProperty, ElementPtr, NotifyTarget, ItemLabel, Index))
		{
			bChanged = true;
		}

		ImGui::SameLine();
		char XId[32];
		snprintf(XId, sizeof(XId), "##rm_%d", Index);
		if (DrawXButton(XId))
		{
			ToRemove = Index;
		}
		ImGui::PopID();
	}

	if (ToRemove >= 0)
	{
		Property.ArrayOps->RemoveAt(ValuePtr, ToRemove);
		bChanged = true;
	}

	char AddLabel[64];
	snprintf(AddLabel, sizeof(AddLabel), "+ Add##%s", Property.Name);
	if (ImGui::Button(AddLabel, ImVec2(-1, 0)))
	{
		Property.ArrayOps->AddDefaulted(ValuePtr);
		bChanged = true;
	}

	ImGui::PopID();
	return bChanged;
}

bool FEditorPropertyWidget::RenderPropertyValueWidget(const FProperty& Property, void* ValuePtr, UObject* NotifyTarget, const char* Label, int32 ArrayIndex)
{
	if (!ValuePtr)
	{
		return false;
	}

	switch (Property.Type)
	{
	case EPropertyType::Bool:
		return ImGui::Checkbox(Label, static_cast<bool*>(ValuePtr));
	case EPropertyType::Int:
		return ImGui::DragInt(Label, static_cast<int32*>(ValuePtr));
	case EPropertyType::Float:
		if (Property.Min != 0.0f || Property.Max != 0.0f)
		{
			return ImGui::DragFloat(Label, static_cast<float*>(ValuePtr), Property.Speed, Property.Min, Property.Max);
		}
		return ImGui::DragFloat(Label, static_cast<float*>(ValuePtr), Property.Speed);
	case EPropertyType::Vec3:
		return ImGui::DragFloat3(Label, static_cast<float*>(ValuePtr), Property.Speed);
	case EPropertyType::Vec4:
		return ImGui::ColorEdit4(Label, static_cast<float*>(ValuePtr));
	case EPropertyType::Color:
		return ImGui::ColorEdit4(Label, &static_cast<FColor*>(ValuePtr)->R);
	case EPropertyType::Guid:
	{
		FGuid* Val = static_cast<FGuid*>(ValuePtr);
		char Buf[64];
		strncpy_s(Buf, sizeof(Buf), Val->ToString().c_str(), _TRUNCATE);
		if (ImGui::InputText(Label, Buf, sizeof(Buf), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			FGuid ParsedGuid;
			if (FGuid::Parse(Buf, ParsedGuid))
			{
				*Val = ParsedGuid;
				return true;
			}
		}
		return false;
	}
	case EPropertyType::Quat:
	{
		FQuat* Val = static_cast<FQuat*>(ValuePtr);
		float Components[4] = { Val->X, Val->Y, Val->Z, Val->W };
		if (ImGui::DragFloat4(Label, Components, Property.Speed))
		{
			*Val = FQuat(Components[0], Components[1], Components[2], Components[3]);
			Val->Normalize();
			return true;
		}
		return false;
	}
	case EPropertyType::ObjectPtr:
		return RenderObjectPtrWidget(Property, ValuePtr, NotifyTarget, Label, ArrayIndex);
	case EPropertyType::SoftObjectPtr:
		return RenderSoftObjectPtrWidget(Property, ValuePtr, Label);
	case EPropertyType::Array:
		return RenderArrayPropertyWidget(Property, ValuePtr, NotifyTarget);
	case EPropertyType::String:
	{
		FString* Val = static_cast<FString*>(ValuePtr);
		if (Property.Name && std::strcmp(Property.Name, "AnimGraphAssetPath") == 0)
		{
			return RenderAnimGraphAssetPathWidget(*Val, Label);
		}

		char Buf[512];
		strncpy_s(Buf, sizeof(Buf), Val->c_str(), _TRUNCATE);
		if (ImGui::InputText(Label, Buf, sizeof(Buf)))
		{
			*Val = Buf;
			return true;
		}
		return false;
	}
	case EPropertyType::Name:
	{
		FName* Val = static_cast<FName*>(ValuePtr);
		FString Current = Val->ToString();
		TArray<FString> Names;
		if (Property.Name && strcmp(Property.Name, "Font") == 0)
		{
			Names = EditorEngine ? EditorEngine->GetAssetService().GetFontNames() : EmptyAssetNames();
		}
		else if (Property.Name && strcmp(Property.Name, "Particle") == 0)
		{
			Names = EditorEngine ? EditorEngine->GetAssetService().GetParticleNames() : EmptyAssetNames();
		}

		if (!Names.empty())
		{
			bool bChanged = false;
			if (ImGui::BeginCombo(Label, Current.c_str()))
			{
				for (const FString& Name : Names)
				{
					const bool bSelected = Current == Name;
					if (ImGui::Selectable(Name.c_str(), bSelected))
					{
						*Val = FName(Name);
						bChanged = true;
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			return bChanged;
		}

		char Buf[256];
		strncpy_s(Buf, sizeof(Buf), Current.c_str(), _TRUNCATE);
		if (ImGui::InputText(Label, Buf, sizeof(Buf)))
		{
			*Val = FName(Buf);
			return true;
		}
		return false;
	}
	case EPropertyType::Enum:
	{
		if (!Property.EnumMeta || !Property.EnumMeta->Values || Property.EnumMeta->Count == 0)
		{
			return false;
		}

		int64 CurrentValue = 0;
		switch (Property.EnumMeta->Size)
		{
		case 1: CurrentValue = static_cast<int64>(*static_cast<uint8*>(ValuePtr)); break;
		case 2: CurrentValue = static_cast<int64>(*static_cast<uint16*>(ValuePtr)); break;
		case 4: CurrentValue = static_cast<int64>(*static_cast<int32*>(ValuePtr)); break;
		case 8: CurrentValue = static_cast<int64>(*static_cast<int64*>(ValuePtr)); break;
		default: break;
		}

		int32 CurrentIndex = 0;
		for (uint32 Index = 0; Index < Property.EnumMeta->Count; ++Index)
		{
			if (Property.EnumMeta->Values[Index].Value == CurrentValue)
			{
				CurrentIndex = static_cast<int32>(Index);
				break;
			}
		}

		const auto ComboGetter = [](void* Data, int Index) -> const char*
		{
			const FEnumMetaData* EnumMeta = static_cast<const FEnumMetaData*>(Data);
			if (!EnumMeta || Index < 0 || static_cast<uint32>(Index) >= EnumMeta->Count)
			{
				return "";
			}
			const FEnumValue& ValueMeta = EnumMeta->Values[Index];
			return (ValueMeta.DisplayName && ValueMeta.DisplayName[0] != '\0') ? ValueMeta.DisplayName : ValueMeta.Name;
		};

		if (ImGui::Combo(Label, &CurrentIndex, ComboGetter, const_cast<FEnumMetaData*>(Property.EnumMeta), static_cast<int>(Property.EnumMeta->Count)))
		{
			const int64 NewValue = Property.EnumMeta->Values[CurrentIndex].Value;
			switch (Property.EnumMeta->Size)
			{
			case 1: *static_cast<uint8*>(ValuePtr) = static_cast<uint8>(NewValue); break;
			case 2: *static_cast<uint16*>(ValuePtr) = static_cast<uint16>(NewValue); break;
			case 4: *static_cast<int32*>(ValuePtr) = static_cast<int32>(NewValue); break;
			case 8: *static_cast<int64*>(ValuePtr) = static_cast<int64>(NewValue); break;
			default: break;
			}
			return true;
		}
		return false;
	}
	default:
		break;
	}
	return false;
}

void FEditorPropertyWidget::RenderPropertyWidget(const FPropertyHandle& Handle)
{
	if (!Handle.IsValid() || !Handle.Property->Name)
	{
		return;
	}

	UObject* Object = Handle.Owner;
	const FProperty& Property = *Handle.Property;
	void* ValuePtr = Handle.GetValuePtr();
	if (!ValuePtr)
	{
		return;
	}

	UObject* NotifyTarget = Object;
	const FString WidgetLabel = MakePropertyWidgetLabel(Property);
	bool bChanged = RenderPropertyValueWidget(Property, ValuePtr, NotifyTarget, WidgetLabel.c_str());

	if (ImGui::IsItemActivated() && !bPropertyEditUndoCaptured && EditorEngine)
	{
		EditorEngine->GetUndoSystem().CaptureSnapshot("Edit Property");
		bPropertyEditUndoCaptured = true;
	}

	if (bChanged && NotifyTarget)
	{
		if (!bPropertyEditUndoCaptured && EditorEngine)
		{
			EditorEngine->GetUndoSystem().CaptureSnapshot("Edit Property");
			bPropertyEditUndoCaptured = true;
		}
		NotifyTarget->PostEditChangeProperty({ Property.Name, EPropertyChangeType::ValueSet });
		if (EditorEngine)
		{
			EditorEngine->GetSceneService().MarkDirty();
		}
	}

	if (ImGui::IsItemDeactivatedAfterEdit() || !ImGui::IsAnyItemActive())
	{
		bPropertyEditUndoCaptured = false;
	}
}


void FEditorPropertyWidget::RenderMaterialPreviewTooltip(UMaterialInterface* Material)
{
	if (!EditorEngine || !Material)
	{
		return;
	}

	ImGui::BeginTooltip();
	ImGui::TextUnformatted(Material->GetName().c_str());
	if (!Material->GetFilePath().empty())
	{
		ImGui::TextDisabled("%s", FPaths::Normalize(Material->GetFilePath()).c_str());
	}
	ImGui::Separator();

	if (UTexture* PreviewTexture = EditorEngine->GetAssetService().GetMaterialPreviewTexture(Material))
	{
		if (ID3D11ShaderResourceView* SRV = PreviewTexture->GetSRV())
		{
			ImGui::Image(reinterpret_cast<ImTextureID>(SRV), ImVec2(128.0f, 128.0f));
			ImGui::Separator();
		}
	}
	else
	{
		ImGui::TextDisabled("No texture preview.");
		ImGui::Separator();
	}

	FMaterialParamValue ParamValue;
	auto DrawColorParam = [](const char* Label, const ImVec4& Color)
	{
		ImGui::ColorButton(Label, Color, ImGuiColorEditFlags_NoTooltip, ImVec2(34.0f, 18.0f));
		ImGui::SameLine();
		ImGui::TextUnformatted(Label);
	};

	if (Material->GetParam("DiffuseColor", ParamValue)
		&& ParamValue.Type == EMaterialParamType::Vector3
		&& std::holds_alternative<FVector>(ParamValue.Value))
	{
		const FVector Color = std::get<FVector>(ParamValue.Value);
		DrawColorParam("Diffuse", ImVec4(Color.X, Color.Y, Color.Z, 1.0f));
	}
	if (Material->GetParam("SpecularColor", ParamValue)
		&& ParamValue.Type == EMaterialParamType::Vector3
		&& std::holds_alternative<FVector>(ParamValue.Value))
	{
		const FVector Color = std::get<FVector>(ParamValue.Value);
		DrawColorParam("Specular", ImVec4(Color.X, Color.Y, Color.Z, 1.0f));
	}
	if (Material->GetParam("EmissiveColor", ParamValue)
		&& ParamValue.Type == EMaterialParamType::Vector3
		&& std::holds_alternative<FVector>(ParamValue.Value))
	{
		const FVector Color = std::get<FVector>(ParamValue.Value);
		DrawColorParam("Emissive", ImVec4(Color.X, Color.Y, Color.Z, 1.0f));
	}
	ImGui::EndTooltip();
}

void FEditorPropertyWidget::RenderSkeletalStateMachinePreview(USkeletalMeshComponent* Comp)
{
	if (!Comp)
	{
		return;
	}

	DrawDetailsSeparator();
	DrawDetailsSectionLabel("Animation StateMachine");

	UAnimationStateMachine* StateMachine = Comp->GetAnimationStateMachine();
	if (!StateMachine)
	{
		ImGui::TextDisabled("No active StateMachine.");
		ImGui::TextDisabled("Run the script to preview states.");
		return;
	}

	const FString CurrentState = StateMachine->GetCurrentStateName();
	const FString NextState = StateMachine->GetNextStateName();
	const bool bBlending = StateMachine->IsBlending();
	const float BlendAlpha = StateMachine->GetBlendAlpha();
	const float BlendElapsed = StateMachine->GetBlendElapsed();
	const float BlendDuration = StateMachine->GetBlendDuration();

	ImGui::Text("Current State: %s", CurrentState.empty() ? "None" : CurrentState.c_str());
	ImGui::Text("Next State: %s", NextState.empty() ? "None" : NextState.c_str());
	
	if (bBlending)
	{
		ImGui::Text("Blending: True (%.2f / %.2fs)", BlendElapsed, BlendDuration);
	}
	else
	{
		ImGui::Text("Blending: False");
	}

	ImGui::ProgressBar(BlendAlpha, ImVec2(-1.0f, 0.0f));

	const TArray<FString> StateNames = StateMachine->GetStateNames();
	if (!StateNames.empty())
	{
		ImGui::Spacing();
		ImGui::TextUnformatted("States");

		if (!StateMachinePreviewBlendTimeByComponent.contains(Comp->GetUUID()))
		{
			StateMachinePreviewBlendTimeByComponent[Comp->GetUUID()] = 0.2f;
		}
		float& PreviewBlendTime = StateMachinePreviewBlendTimeByComponent[Comp->GetUUID()];

		ImGui::DragFloat("Preview Blend Time", &PreviewBlendTime, 0.01f, 0.0f, 10.0f, "%.2f");

		const float ButtonWidth = 88.0f;
		const float Spacing = ImGui::GetStyle().ItemSpacing.x;
		const float Available = ImGui::GetContentRegionAvail().x;
		int32 ButtonsInRow = 0;

		for (const FString& StateName : StateNames)
		{
			ImGui::PushID(StateName.c_str());

			if (ButtonsInRow > 0 && ButtonsInRow * (ButtonWidth + Spacing) + ButtonWidth <= Available)
			{
				ImGui::SameLine();
			}
			else
			{
				ButtonsInRow = 0;
			}

			const bool bIsCurrentState = (StateName == CurrentState);
			if (bIsCurrentState)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.25f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.30f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.40f, 0.22f, 1.0f));
			}

			if (ImGui::Button(StateName.c_str(), ImVec2(ButtonWidth, 0.0f)))
			{
				StateMachine->SetStateByName(StateName, PreviewBlendTime);
			}

			if (bIsCurrentState)
			{
				ImGui::PopStyleColor(3);
			}

			++ButtonsInRow;
			ImGui::PopID();
		}
	}

	const TArray<FAnimTransitionDebugInfo> Transitions = StateMachine->GetTransitionDebugInfos();
	if (!Transitions.empty())
	{
		ImGui::Spacing();
		ImGui::TextUnformatted("Transitions");

		if (ImGui::BeginTable("##StateMachineTransitions", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("From");
			ImGui::TableSetupColumn("To");
			ImGui::TableSetupColumn("Blend");
			ImGui::TableHeadersRow();

			for (const FAnimTransitionDebugInfo& Transition : Transitions)
			{
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(Transition.FromState.c_str());

				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(Transition.ToState.c_str());

				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%.2fs", Transition.BlendTime);
			}

			ImGui::EndTable();
		}
	}
}

void FEditorPropertyWidget::RenderSkeletalBonePoseDebug(USkeletalMeshComponent* Comp)
{
	const FDetailsPerfClock::time_point DebugStart = bDetailsPerfTraceFrame ? FDetailsPerfClock::now() : FDetailsPerfClock::time_point{};
	if (!Comp)
	{
		return;
	}

	USkeletalMesh* Mesh = Comp->GetSkeletalMesh();
	if (!Mesh)
	{
		return;
	}

	const TArray<FBoneInfo>& Bones = Mesh->GetBones();
	if (Bones.empty())
	{
		return;
	}

	const uint32 ComponentId = Comp->GetUUID();
	int32& SelectedBoneIndex = SelectedSkeletalBoneByComponent[ComponentId];
	if (SelectedBoneIndex < 0 || SelectedBoneIndex >= static_cast<int32>(Bones.size()))
	{
		SelectedBoneIndex = 0;
	}

	DrawDetailsSeparator();
	DrawDetailsSectionLabel("Bone Pose Debug");
	ImGui::Spacing();

	ImGui::PushID(Comp);

	const auto MakeBoneLabel = [&Bones](int32 BoneIndex) -> FString
	{
		if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Bones.size()))
		{
			return "None";
		}

		return std::to_string(BoneIndex) + ": " + Bones[BoneIndex].Name;
	};

	const FString CurrentLabel = MakeBoneLabel(SelectedBoneIndex);
	if (ImGui::BeginCombo("Bone", CurrentLabel.c_str()))
	{
		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
		{
			const FString Label = MakeBoneLabel(BoneIndex);
			const bool bSelected = SelectedBoneIndex == BoneIndex;
			if (ImGui::Selectable(Label.c_str(), bSelected))
			{
				SelectedBoneIndex = BoneIndex;
			}

			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}

		ImGui::EndCombo();
	}

	const uint32 MeshId = Mesh->GetUUID();
	TMap<int32, FSkeletalBonePoseEditState>& BonePoseEditStates = SkeletalBonePoseEditStatesByComponent[ComponentId];
	FSkeletalBonePoseEditState& EditState = BonePoseEditStates[SelectedBoneIndex];

	const auto ResetEditStateToIdentityOffset = [](FSkeletalBonePoseEditState& State, uint32 InMeshId, int32 InBoneIndex)
	{
		State.MeshId = InMeshId;
		State.BoneIndex = InBoneIndex;
		State.LocationOffset = FVector::ZeroVector;
		State.RotationOffset = FVector::ZeroVector;
		State.ScaleOffset = FVector::OneVector;
	};

	const auto InitializeEditStateFromCurrentPose = [Comp, Mesh, MeshId](FSkeletalBonePoseEditState& State, int32 BoneIndex)
	{
		State.MeshId = MeshId;
		State.BoneIndex = BoneIndex;

		const FMatrix& BindLocalTransform = Mesh->GetLocalBindTransform(BoneIndex);
		const FMatrix CurrentLocalTransform = Comp->GetBoneLocalTransform(BoneIndex);
		const FMatrix OffsetTransformMatrix = CurrentLocalTransform * BindLocalTransform.GetInverse();
		const FTransform OffsetTransform(OffsetTransformMatrix);

		State.LocationOffset = OffsetTransform.GetTranslation();
		State.RotationOffset = OffsetTransform.Rotator().Euler();
		State.ScaleOffset = OffsetTransform.GetScale3D();
	};

	if (EditState.MeshId != MeshId || EditState.BoneIndex != SelectedBoneIndex)
	{
		InitializeEditStateFromCurrentPose(EditState, SelectedBoneIndex);
	}

	auto DrawVec3 = [this](const char* Label, FVector& Value, float Speed) -> bool
	{
		float Values[3] = { Value.X, Value.Y, Value.Z };
		const bool bEdited = ImGui::DragFloat3(Label, Values, Speed);
		if (ImGui::IsItemActivated() && EditorEngine)
		{
			EditorEngine->GetUndoSystem().CaptureSnapshot("Edit Bone Pose");
		}

		if (bEdited)
		{
			Value = FVector(Values[0], Values[1], Values[2]);
		}

		return bEdited;
	};

	const bool bTranslationEdited = DrawVec3("Location Offset", EditState.LocationOffset, 0.1f);
	const bool bRotationEdited = DrawVec3("Rotation Offset", EditState.RotationOffset, 0.1f);
	const bool bScaleEdited = DrawVec3("Scale Offset", EditState.ScaleOffset, 0.01f);

	if (bTranslationEdited || bRotationEdited || bScaleEdited)
	{
		const FTransform OffsetTransform(
			FQuat::MakeFromEuler(EditState.RotationOffset),
			EditState.LocationOffset,
			EditState.ScaleOffset);
		const FMatrix NewLocalTransform =
			OffsetTransform.ToMatrixWithScale() * Mesh->GetLocalBindTransform(SelectedBoneIndex);
		Comp->SetBoneLocalTransform(SelectedBoneIndex, NewLocalTransform);
	}

	const float HalfWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
	if (ImGui::Button("Reset Bone", ImVec2(HalfWidth, 0.0f)))
	{
		if (EditorEngine)
		{
			EditorEngine->GetUndoSystem().CaptureSnapshot("Reset Bone Pose");
		}
		ResetEditStateToIdentityOffset(EditState, MeshId, SelectedBoneIndex);
		Comp->SetBoneLocalTransform(SelectedBoneIndex, Mesh->GetLocalBindTransform(SelectedBoneIndex));
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset Pose", ImVec2(-1.0f, 0.0f)))
	{
		if (EditorEngine)
		{
			EditorEngine->GetUndoSystem().CaptureSnapshot("Reset Bone Pose");
		}
		Comp->ResetToBindPose();
		BonePoseEditStates.clear();
	}

	ImGui::PopID();

	if (bDetailsPerfTraceFrame)
	{
		UE_LOG(
			"[DetailsPerf] SkeletalBoneDebug Bones=%zu SelectedBone=%d Time=%.2fms",
			Bones.size(),
			SelectedBoneIndex,
			DetailsPerfMs(DebugStart, FDetailsPerfClock::now()));
	}
}

void FEditorPropertyWidget::RenderInterpControlPoints(UInterpToMovementComponent* Comp)
{
	// --- Playback actions -----------------------------------------------
	DrawDetailsSeparator();
	DrawDetailsSectionLabel("Playback");
	ImGui::Spacing();

	float HalfWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
	if (ImGui::Button("Initiate", ImVec2(HalfWidth, 0))) Comp->Initiate();
	ImGui::SameLine();
	if (ImGui::Button("Stop",     ImVec2(HalfWidth, 0))) Comp->ResetAndHalt();
	if (ImGui::Button("Reset",    ImVec2(-1,        0))) Comp->Reset();
}


void FEditorPropertyWidget::AttachAndSelectNewComponent(AActor* PrimaryActor, UActorComponent* NewComp, USceneComponent* AttachTargetOverride)
{
	if (!PrimaryActor || !NewComp) return;

	USceneComponent* AttachTarget = nullptr;
	if (AttachTargetOverride && AttachTargetOverride->GetOwner() == PrimaryActor)
	{
		AttachTarget = AttachTargetOverride;
	}
	else if (SelectedComponent && SelectedComponent->IsA<USceneComponent>() && SelectedComponent->GetOwner() == PrimaryActor)
	{
		AttachTarget = static_cast<USceneComponent*>(SelectedComponent);
	}
	else
	{
		AttachTarget = PrimaryActor->GetRootComponent();
	}

	if (USceneComponent* SceneComp = Cast<USceneComponent>(NewComp))
	{
		if (AttachTarget && SceneComp != AttachTarget)
		{
			SceneComp->AttachToComponent(AttachTarget);
		}
		else if (!PrimaryActor->GetRootComponent())
		{
			PrimaryActor->SetRootComponent(SceneComp);
		}
	}
	else if (UMovementComponent* MoveComp = Cast<UMovementComponent>(NewComp))
	{
		if (AttachTarget) MoveComp->SetUpdatedComponent(AttachTarget);
	}

	if (UScriptComponent* ScriptComp = Cast<UScriptComponent>(NewComp))
	{
		if (ScriptComp->GetScriptName().empty())
		{
			FString SceneName = "Default";
			if (EditorEngine)
			{
				SceneName = EditorEngine->GetSceneService().GetSceneName();
			}
			ScriptComp->SetScriptName(MakeDefaultScriptName(SceneName, PrimaryActor));
		}
	}

	SelectComponentForDetails(NewComp);
}

template<typename T>
void FEditorPropertyWidget::RenderEditableName(const char* Label, T* TargetObject, bool* bFocusNextFrame)
{
	if (!TargetObject) return;

	char NameBuf[256];
	strncpy_s(NameBuf, sizeof(NameBuf), TargetObject->GetFName().ToString().c_str(), _TRUNCATE);

	if (bFocusNextFrame && *bFocusNextFrame)
	{
		ImGui::SetKeyboardFocusHere();
		*bFocusNextFrame = false;
	}

	// Enter 키를 누르거나 포커스를 잃었을 경우에 이름이 변경되도록 설정
	if (ImGui::InputText(Label, NameBuf, sizeof(NameBuf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
	{
		if (EditorEngine)
		{
			EditorEngine->GetUndoSystem().CaptureSnapshot("Rename");
		}
		TargetObject->SetFName(FName(NameBuf));
		if (EditorEngine)
		{
			EditorEngine->GetSceneService().MarkDirty();
		}
	}
}
