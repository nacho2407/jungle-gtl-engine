#include "Editor/UI/EditorPropertyWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/EditorRenderPipeline.h"
#include "ImGui/imgui.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"
#include "Component/StaticMeshComponent.h"
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
#include "Core/PropertyTypes.h"
#include "Core/Paths.h"
#include "Math/Color.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/Material.h"
#include "Asset/StaticMesh.h"
#include "Object/FName.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
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

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

namespace UIConstants
{
	constexpr float XButtonSize    = 20.0f;
	constexpr float MinScrollHeight = 50.0f;
}

namespace
{
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
                TargetName = UpdatedComp->GetTypeInfo()->name;
            }
            return FString("MC_") + TargetName;
        }

        // 대상이 없는 경우
        FString DefaultName = MoveComp->GetFName().ToString();
        if (DefaultName.empty())
        {
            DefaultName = MoveComp->GetTypeInfo()->name;
        }
        return DefaultName;
    }

	static FString MakeDefaultScriptName(const FString& SceneName, AActor* Actor)
    {
        FString ActorName = "Actor";
        FString ValidSceneName = SceneName.empty() ? "Default" : SceneName;

        if (Actor)
        {
            const FTypeInfo* TypeInfo = Actor->GetTypeInfo();
            ActorName = TypeInfo ? TypeInfo->name : "Actor";
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
 }

// 1. 메뉴 항목의 이름과, 해당 컴포넌트를 생성&초기화할 함수(람다)를 담는 구조체
struct FComponentMenuEntry
{
	const char* DisplayName;
	std::function<UActorComponent*(AActor*)> CreateAndInitFunc;
};

// 2. 에디터에서 추가 가능한 컴포넌트 배열 (이 리스트만 관리하면 됩니다)
static const TArray<FComponentMenuEntry> ComponentMenuRegistry = {
	{
		"Scene Component",
		[](AActor* Actor) -> UActorComponent* {
			return Actor->AddComponent<USceneComponent>();
		}
	},
	{
		"StaticMesh Component",
		[](AActor* Actor) -> UActorComponent* {
			return Actor->AddComponent<UStaticMeshComponent>();
		}
	},
	{
		"SubUV Component",
		[](AActor* Actor) -> UActorComponent* {
			USubUVComponent* Comp = Actor->AddComponent<USubUVComponent>();
			Comp->SetParticle(FName("Explosion"));
			Comp->SetSpriteSize(2.0f, 2.0f);
			Comp->SetFrameRate(30.f);
			return Comp;
		}
	},
	{
		"TextRender Component",
		[](AActor* Actor) -> UActorComponent* {
			UTextRenderComponent* Comp = Actor->AddComponent<UTextRenderComponent>();
			Comp->SetFont(FName("Default"));
			Comp->SetText("TextRender");
			return Comp;
		}
	},
	{
		"Billboard Component",
		[](AActor* Actor) -> UActorComponent* {
			UBillboardComponent* Comp = Actor->AddComponent<UBillboardComponent>();
			Comp->SetTextureName("Asset/Texture/Pawn_64x.png");
			return Comp;
		}
	},
	{
		"SpringArm Component",
		[](AActor* Actor) -> UActorComponent* {
			USpringArmComponent* Comp = Actor->AddComponent<USpringArmComponent>();
			Comp->SetRelativeLocation(FVector(0.0f, 0.0f, 1.6f));
			return Comp;
		}
	},
	{
		"Camera Component",
		[](AActor* Actor) -> UActorComponent* {
			UCameraComponent* Comp = Actor->AddComponent<UCameraComponent>();
			return Comp;
		}
	},
	{
		"Sound Component",
		[](AActor* Actor) -> UActorComponent* {
			USoundComponent* Comp = Actor->AddComponent<USoundComponent>();
			return Comp;
		}
	},
	{
		"RotatingMovement Component",
		[](AActor* Actor) -> UActorComponent* {
			URotatingMovementComponent* Comp = Actor->AddComponent<URotatingMovementComponent>();
			return Comp;
		}
	},
    {
		"InterpToMovement Component",
		[](AActor* Actor) -> UActorComponent* {
          UInterpToMovementComponent* Comp = Actor->AddComponent<UInterpToMovementComponent>();
          return Comp;
		}
	},
    {
		"PursuitMovement Component",
		[](AActor* Actor) -> UActorComponent* {
			UPursuitMovementComponent* Comp = Actor->AddComponent<UPursuitMovementComponent>();
			return Comp;
		}
	},
	{
		"ProjectileMovement Component",
		[](AActor* Actor) -> UActorComponent* {
			UProjectileMovementComponent* Comp = Actor->AddComponent<UProjectileMovementComponent>();
			return Comp;
		}
	},
	{
		"HeightFog Component",
		[](AActor* Actor) -> UActorComponent* {
			UHeightFogComponent* Comp = Actor->AddComponent<UHeightFogComponent>();
			Comp->SetFogDensity(0);
			Comp->SetFogInscatteringColor(FVector4(0.72f, 0.8f, 0.9f, 1.0f));
			Comp->SetHeightFalloff(0);
			Comp->SetFogHeight(0);
			return Comp;
		}
	},
	
	{
		"Fireball Component",
		[](AActor* Actor) -> UActorComponent* {
			UFireballComponent* Comp = Actor->AddComponent<UFireballComponent>();
			return Comp;
		}
	},

	{
		"PointLight Component",
		[](AActor* Actor) -> UActorComponent* {
			UPointLightComponent* Comp = Actor->AddComponent<UPointLightComponent>();
			return Comp;
		}
	},

	{ 
		"Box Component",
		[](AActor* Actor) -> UActorComponent*
		{
			UBoxComponent* Comp = Actor->AddComponent<UBoxComponent>();
			return Comp;
		} 
	},

	{ 
		"Sphere Component",
		[](AActor* Actor) -> UActorComponent*
		{
			USphereComponent* Comp = Actor->AddComponent<USphereComponent>();
			return Comp;
		} 
	},

	{ 
		"Capsule Component",
		[](AActor* Actor) -> UActorComponent*
		{
			UCapsuleComponent* Comp = Actor->AddComponent<UCapsuleComponent>();
			return Comp;
		} 
	},
	{
		"Script Component",
		[](AActor* Actor) -> UActorComponent*
		{
            UScriptComponent* Comp = Actor->AddComponent<UScriptComponent>();
			return Comp;
		} 
	},
	{
		"ActorSequence Component",
		[](AActor* Actor) -> UActorComponent*
		{
			UActorSequenceComponent* Comp = Actor->AddComponent<UActorSequenceComponent>();
			return Comp;
		}
	},
};

void FEditorPropertyWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
	SelectionManager = &EditorEngine->GetSelectionManager();
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

	AActor* CurrentSelection = SelectionManager->GetPrimarySelection();
	if (!IsLiveActor(CurrentSelection))
	{
		SelectionManager->ClearSelection();
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

	const TArray<AActor*>& SelectedActors = SelectionManager->GetSelectedActors();
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

	if (!bDetailsLocked && SelectionManager->GetPrimarySelection() == nullptr)
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
		if (LockedName.empty()) LockedName = DisplayActor->GetTypeInfo()->name;
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
	if (PrimaryActor != LastSelectedActor)
	{
		SelectedComponent = nullptr;
		LastSelectedActor = PrimaryActor;
		bActorSelected = true;
	}

	if (!bDetailsLocked && SelectionManager)
	{
		SelectionManager->ValidateSelection();
		UActorComponent* ManagerComponent = SelectionManager->GetSelectedComponent();
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
	bActorSelected = true;
	SelectedComponent = nullptr;
	if (SelectionManager)
	{
		SelectionManager->ClearComponentSelection();
	}
}

void FEditorPropertyWidget::SelectComponentForDetails(UActorComponent* Component)
{
	SelectedComponent = Component;
	bActorSelected = false;
	if (SelectionManager)
	{
		SelectionManager->SelectComponent(Component);
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
	ImGui::Text("Class: %s", PrimaryActor->GetTypeInfo()->name);

	FString PrimaryName = PrimaryActor->GetFName().ToString();
	if (PrimaryName.empty()) PrimaryName = PrimaryActor->GetTypeInfo()->name;

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
		for (const FComponentMenuEntry& Entry : ComponentMenuRegistry)
		{
			if (ImGui::Selectable(Entry.DisplayName))
			{
				if (EditorEngine)
				{
					EditorEngine->GetUndoSystem().CaptureSnapshot("Add Component");
				}
				if (UActorComponent* NewComp = Entry.CreateAndInitFunc(PrimaryActor))
				{
					AttachAndSelectNewComponent(PrimaryActor, NewComp, AddAttachTarget);
					if (EditorEngine)
					{
						EditorEngine->GetMainPanel().GetSceneWidget().MarkSceneDirty();
					}
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
    if (ActorName.empty()) ActorName = Actor->GetTypeInfo()->name;

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
    if (Name.empty()) Name = Comp->GetTypeInfo()->name;

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

	UActorComponent* ComponentToDelete = SelectedComponent;
	if (EditorEngine)
	{
		EditorEngine->GetUndoSystem().CaptureSnapshot("Delete Component");
	}
	SelectedComponent = nullptr;
	bActorSelected = true;
	if (SelectionManager)
	{
		SelectionManager->OnComponentDestroyed(ComponentToDelete);
	}
	Owner->RemoveComponent(ComponentToDelete);
	if (EditorEngine)
	{
		EditorEngine->GetMainPanel().GetSceneWidget().MarkSceneDirty();
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
	ImGui::Text("Actor: %s", PrimaryActor->GetTypeInfo()->name);
	RenderEditableName("Name##Actor", PrimaryActor, &bFocusActorNameNextFrame); // 편집 가능한 UI
	RenderActorTags(PrimaryActor, SelectedActors);

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
				EditorEngine->GetGizmo()->UpdateGizmoTransform();
			}
		};

		// Location, Rotation, Scale을 한 번에 그려줍니다.
		DrawTransformField("Location", PrimaryActor->GetActorLocation(), [](AActor* A, FVector D) { A->AddActorWorldOffset(D); });
		DrawTransformField("Rotation", PrimaryActor->GetActorRotation(), [](AActor* A, FVector D) { A->SetActorRotation(A->GetActorRotation() + D); });
		DrawTransformField("Scale",    PrimaryActor->GetActorScale(),    [](AActor* A, FVector D) { A->SetActorScale(A->GetActorScale() + D); });
	}

	DrawDetailsSeparator();
	bool bVisible = PrimaryActor->IsVisible();
	const bool bVisibleEdited = ImGui::Checkbox("Visible", &bVisible);
	if (ImGui::IsItemActivated() && EditorEngine)
	{
		EditorEngine->GetUndoSystem().CaptureSnapshot("Edit Actor");
	}
	if (bVisibleEdited)
	{
		PrimaryActor->SetVisible(bVisible);
	}

	DrawDetailsSeparator();
	// Billboard 타입 체크
	if (UBillboardComponent* BillboardComp = dynamic_cast<UBillboardComponent*>(PrimaryActor->GetRootComponent()))
	{
		if (dynamic_cast<USubUVComponent*>(PrimaryActor->GetRootComponent()))
		{
			return;
		}
		DrawDetailsSeparator();
		DrawDetailsSectionLabel("Sprite Texture");

		const TArray<FString>& TextureList = FResourceManager::Get().GetTextureFilePath();
		const FString CurrentName = BillboardComp->GetTextureName();

		if (ImGui::BeginCombo("##SpriteTexture", CurrentName.empty() ? "None" : CurrentName.c_str()))
		{
			for (const FString& TexPath : TextureList)
			{
				// 경로 전체 대신 파일명만 표시
				FString DisplayName = TexPath;
				bool bSelected = (TexPath == CurrentName);

				if (ImGui::Selectable(DisplayName.c_str(), bSelected))
				{
					if (EditorEngine)
					{
						EditorEngine->GetUndoSystem().CaptureSnapshot("Edit Billboard");
					}
					for (AActor* Actor : SelectedActors)
					{
						if (UBillboardComponent* Comp =
							dynamic_cast<UBillboardComponent*>(Actor->GetRootComponent()))
						{
							Comp->SetTextureName(TexPath);
						}
					}
				}
				if (bSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	if (PrimaryActor->IsA<ADecalSpotLightActor>())
	{
		ADecalSpotLightActor* SpotActor = static_cast<ADecalSpotLightActor*>(PrimaryActor);
		DrawDetailsSeparator();
		DrawDetailsSectionLabel("Spot Light Properties");
		float Range = SpotActor->GetRange();
		const bool bRangeEdited = ImGui::DragFloat("Range", &Range, 0.1f, 0.0f, 1000.0f);
		if (ImGui::IsItemActivated() && EditorEngine)
		{
			EditorEngine->GetUndoSystem().CaptureSnapshot("Edit Light");
		}
		if (bRangeEdited)
		{
			for (AActor* Actor : SelectedActors)
			{
				if (ADecalSpotLightActor* SA = dynamic_cast<ADecalSpotLightActor*>(Actor))
				{
					SA->SetRange(Range);
				}
			}
		}
		float Angle = SpotActor->GetAngle();
		const bool bAngleEdited = ImGui::DragFloat("Angle", &Angle, 0.1f, 0.0f, 180.0f);
		if (ImGui::IsItemActivated() && EditorEngine)
		{
			EditorEngine->GetUndoSystem().CaptureSnapshot("Edit Light");
		}
		if (bAngleEdited)
		{
			for (AActor* Actor : SelectedActors)
			{
				if (ADecalSpotLightActor* SA = dynamic_cast<ADecalSpotLightActor*>(Actor))
				{
					SA->SetAngle(Angle);
				}
			}
		}
	}
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
					EditorEngine->GetMainPanel().GetSceneWidget().MarkSceneDirty();
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
				EditorEngine->GetMainPanel().GetSceneWidget().MarkSceneDirty();
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

	DrawDetailsSeparator();
	DrawDetailsSectionLabel("Component Tags");

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
						EditorEngine->GetMainPanel().GetSceneWidget().MarkSceneDirty();
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
				EditorEngine->GetMainPanel().GetSceneWidget().MarkSceneDirty();
			}
		}
	}
}

void FEditorPropertyWidget::RenderComponentProperties()
{
	ImGui::Text("Component: %s", SelectedComponent->GetTypeInfo()->name);
	RenderEditableName("Name##Component", SelectedComponent, &bFocusComponentNameNextFrame); // 편집 가능한 UI
	RenderComponentTags(SelectedComponent);

	DrawDetailsSeparator();

	// PropertyDescriptor 기반 자동 위젯 렌더링
	TArray<FPropertyDescriptor> Props;
	SelectedComponent->GetEditableProperties(Props);

	AActor* Owner = SelectedComponent->GetOwner();

	for (auto& Prop : Props)
    {
        if (!Prop.Name)
        {
            continue;
        }

		if (strcmp(Prop.Name, "Tags") == 0)
		{
			continue;
		}

        const bool bIsScriptName =
            strcmp(Prop.Name, "ScriptName") == 0;

        FString OldScriptName;

        if (bIsScriptName)
        {
            if (FString* ScriptNamePtr = static_cast<FString*>(Prop.ValuePtr))
            {
                OldScriptName = *ScriptNamePtr;
            }
        }

        if (Prop.Type == EPropertyType::SceneComponentRef)
        {
            RenderSceneComponentRefWidget(Prop, Owner);
        }
        else
        {
            RenderPropertyWidget(Prop);
        }

        if (bIsScriptName)
        {
            UScriptComponent* ScriptComp = Cast<UScriptComponent>(SelectedComponent);
            if (!ScriptComp)
            {
                return;
            }

            const FString& NewScriptName = ScriptComp->GetScriptName();

            if (OldScriptName != NewScriptName)
            {
                // ScriptName 변경으로 PostEditProperty -> ReloadLuaProperties가 실행됨.
                // 기존 Props 안의 Lua property descriptor는 이제 무효이므로
                // 이번 프레임 Details 렌더링을 중단한다.
                return;
            }
        }
    }
	// Special: InterpToMovementComponent control points + behaviour + actions
	if (UInterpToMovementComponent* InterpComp = Cast<UInterpToMovementComponent>(SelectedComponent))
	{
		RenderInterpControlPoints(InterpComp);
	}

	if (UActorSequenceComponent* SequenceComp = Cast<UActorSequenceComponent>(SelectedComponent))
	{
		ActorSequenceDetails.Render(SequenceComp, LastDeltaTime);
	}

	if (ULightComponent* LightComp = Cast<ULightComponent>(SelectedComponent))
	{
		if (ImGui::Button("Override camera with light's perspective"))
		{
			UWorld* World = GEngine ? GEngine->GetWorld() : nullptr;
			FViewportCamera* Camera = World ? World->GetActiveCamera() : nullptr;
			if (Camera)
			{
				Camera->SetLocation(LightComp->GetWorldLocation());
				Camera->SetRotation(LightComp->GetRelativeQuat());
			}
		}
    }
    else if (UScriptComponent* ScriptComp = Cast<UScriptComponent>(SelectedComponent))
    {
        FScriptManager& ScriptMgr = FScriptManager::Get();
        if (ImGui::Button("Create Script"))
		{
            FString ScriptPath = ScriptComp->GetScriptName();
            if (ScriptPath.empty() || IsBlankString(ScriptPath))
            {
                if (EditorEngine)
                {
                    EditorEngine->GetMainPanel().PushFooterLog("Script name is empty");
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
                        EditorEngine->GetMainPanel().PushFooterLog("Script create failed");
                    }
                    return;
                }

                ScriptComp->SetScriptName(MakeScriptReferenceFromPath(SelectedScriptPath));
                ScriptComp->ReloadLuaProperties();
                if (EditorEngine)
                {
                    EditorEngine->GetMainPanel().PushFooterLog("Script created");
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
                    EditorEngine->GetMainPanel().PushFooterLog("No script selected");
                }
            }
            else if (!ScriptMgr.EditScript(ScriptPath) && EditorEngine)
            {
                EditorEngine->GetMainPanel().PushFooterLog("Script file not found");
            }
        }
	}
	ImGui::Separator();

	// 프로퍼티 직접 편집 후 월드 행렬 갱신
	if (SelectedComponent->IsA<USceneComponent>())
	{
		static_cast<USceneComponent*>(SelectedComponent)->MarkTransformDirty();
		SelectionManager->GetGizmo()->UpdateGizmoTransform();
	}
}

void FEditorPropertyWidget::RenderSceneComponentRefWidget(FPropertyDescriptor& Prop, AActor* Owner)
{
	// ValuePtr은 USceneComponent* 변수의 주소 (USceneComponent**)
	USceneComponent** ValuePtr = reinterpret_cast<USceneComponent**>(Prop.ValuePtr);
	USceneComponent* CurrentComp = *ValuePtr;

	// 액터 소유 SceneComponent 목록 수집
	TArray<USceneComponent*> SceneComps;
	SceneComps.push_back(nullptr); // "None" 선택지
	if (Owner)
	{
		for (UActorComponent* Comp : Owner->GetComponents())
		{
			if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
				SceneComps.push_back(SceneComp);
		}
	}

	// 드롭다운 레이블 생성: "[Root] ClassName" 또는 "ClassName [FName]"
	auto GetLabel = [&](USceneComponent* Comp) -> FString {
		if (!Comp) return "None";
		FString Name = Comp->GetFName().ToString();
		if (Name.empty()) Name = Comp->GetTypeInfo()->name;
		bool bIsRoot = Owner && (Comp == Owner->GetRootComponent());
		return bIsRoot ? ("[Root] " + Name) : Name;
	};

	FString CurrentLabel = GetLabel(CurrentComp);
	if (ImGui::BeginCombo(Prop.Name, CurrentLabel.c_str()))
	{
		for (USceneComponent* SceneComp : SceneComps)
		{
			bool bSelected = (SceneComp == CurrentComp);
			// ##ptr 으로 포인터를 ID로 사용하여 동일 이름 컴포넌트를 구별
			char SelectableId[128];
			snprintf(SelectableId, sizeof(SelectableId), "%s##%p",
				GetLabel(SceneComp).c_str(), static_cast<void*>(SceneComp));
			if (ImGui::Selectable(SelectableId, bSelected))
			{
				if (EditorEngine)
				{
					EditorEngine->GetUndoSystem().CaptureSnapshot("Edit Component Reference");
				}
				*ValuePtr = SceneComp;
				SelectedComponent->PostEditChangeProperty({ Prop.Name, EPropertyChangeType::ValueSet });
			}
			if (bSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}

void FEditorPropertyWidget::RenderPropertyWidget(FPropertyDescriptor& Prop)
{
	bool bChanged = false;

	switch (Prop.Type)
	{
	case EPropertyType::Bool:
	{
		bool* Val = static_cast<bool*>(Prop.ValuePtr);
		bChanged = ImGui::Checkbox(Prop.Name, Val);
		break;
	}
	case EPropertyType::Int:
	{
		int32* Val = static_cast<int32*>(Prop.ValuePtr);
		bChanged = ImGui::DragInt(Prop.Name, Val);
		break;
	}
	case EPropertyType::Float:
	{
		float* Val = static_cast<float*>(Prop.ValuePtr);
		if (Prop.Min != 0.0f || Prop.Max != 0.0f)
			bChanged = ImGui::DragFloat(Prop.Name, Val, Prop.Speed, Prop.Min, Prop.Max);
		else
			bChanged = ImGui::DragFloat(Prop.Name, Val, Prop.Speed);
		break;
	}
	case EPropertyType::Vec3:
	{
		float* Val = static_cast<float*>(Prop.ValuePtr);
		bChanged = ImGui::DragFloat3(Prop.Name, Val, Prop.Speed);
		break;
	}
	case EPropertyType::Vec4:
	{
		float* Val = static_cast<float*>(Prop.ValuePtr);
		bChanged = ImGui::ColorEdit4(Prop.Name, Val);
		break;
	}
	case EPropertyType::Color:
	{
		FColor* Val = static_cast<FColor*>(Prop.ValuePtr);
		bChanged = ImGui::ColorEdit4(Prop.Name, &Val->R);
		break;
	}
	case EPropertyType::String:
	{
		FString* Val = static_cast<FString*>(Prop.ValuePtr);

		if (strcmp(Prop.Name, "StaticMesh") == 0)
		{
			TArray<FString> MeshPaths = FResourceManager::Get().GetStaticMeshPaths();
			if (!MeshPaths.empty())
			{
				const FString Current = *Val;
				if (ImGui::BeginCombo(Prop.Name, Current.empty() ? "<None>" : Current.c_str()))
				{
					for (const FString& Path : MeshPaths)
					{
						const bool bSelected = (Current == Path);
						if (ImGui::Selectable(Path.c_str(), bSelected))
						{
							*Val = Path;
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
		}
        else if (strcmp(Prop.Name, "ScriptName") == 0)
        {
            if (!Val)
            {
                return;
            }

            FScriptManager::Get().RefreshLuaScriptFiles();

            TMap<FName, FLuaScriptInfo, FName::Hash>& ScriptArray =
                FScriptManager::Get().GetScriptArray();

            ImGui::PushID(Val);

            char Buffer[512] = {};
            strncpy_s(Buffer, sizeof(Buffer), Val->c_str(), _TRUNCATE);

            if (ImGui::InputText("##ScriptPathInput", Buffer, sizeof(Buffer)))
            {
                *Val = Buffer;
                bChanged = true;
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("LuaScriptContentItem"))
                {
                    const char* PayloadPath = static_cast<const char*>(Payload->Data);
                    if (PayloadPath && PayloadPath[0] != '\0')
                    {
                        // 중요:
                        // MakeScriptReferenceFromPath()는 반드시 FScriptManager의 ScriptArray key와
                        // 같은 형식의 문자열을 반환해야 한다.
                        FString NewScriptRef = MakeScriptReferenceFromPath(PayloadPath);

                        if (!NewScriptRef.empty())
                        {
                            *Val = NewScriptRef;
                            bChanged = true;
                        }
                    }
                }

                ImGui::EndDragDropTarget();
            }

            ImGui::SameLine();

            const FString Current = *Val;

            FString Preview = "SelectScript";

            for (const auto& [ScriptName, ScriptInfo] : ScriptArray)
            {
                const FString Key = ScriptName.ToString();

                if (Key == Current)
                {
                    if (!ScriptInfo.ScriptPath.empty())
                    {
                        Preview = FPaths::ToRelativeString(ScriptInfo.ScriptPath);
                    }

                    if (Preview.empty())
                    {
                        Preview = Key;
                    }

                    break;
                }
            }

            if (ImGui::BeginCombo("##ScriptPathCombo", Preview.c_str()))
            {
                for (const auto& [ScriptName, ScriptInfo] : ScriptArray)
                {
                    const FString Key = ScriptName.ToString();

                    if (Key.empty())
                    {
                        continue;
                    }

                    if (ScriptInfo.ScriptPath.empty())
                    {
                        continue;
                    }

                    if (!std::filesystem::exists(ScriptInfo.ScriptPath))
                    {
                        continue;
                    }

                    FString RelativePath = FPaths::ToRelativeString(ScriptInfo.ScriptPath);

                    // ImGui label은 절대 빈 문자열이면 안 됨
                    FString DisplayName = RelativePath.empty() ? Key : RelativePath;

                    const bool bSelected = (Current == Key);

                    ImGui::PushID(Key.c_str());

                    if (ImGui::Selectable(DisplayName.c_str(), bSelected))
                    {
                        // 저장은 표시용 RelativePath가 아니라 ScriptManager key로 한다.
                        *Val = Key;
                        bChanged = true;
                    }

                    ImGui::PopID();

                    if (bSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }

            const FString NewCurrent = *Val;

            if (!NewCurrent.empty() && !FScriptManager::Get().HasScript(FName(NewCurrent)))
            {
                ImGui::TextColored(
                    ImVec4(1.0f, 0.42f, 0.35f, 1.0f),
                    "Missing script file.");

                ImGui::SameLine();

                if (ImGui::SmallButton("Clear##MissingScript"))
                {
                    Val->clear();
                    bChanged = true;
                }
            }

            ImGui::PopID();
        }
		else
		{
			char Buf[256];
			strncpy_s(Buf, sizeof(Buf), Val->c_str(), _TRUNCATE);
			if (ImGui::InputText(Prop.Name, Buf, sizeof(Buf)))
			{
				*Val = Buf;
				bChanged = true;
			}
		}
		break;
	}
	case EPropertyType::Name:
	{
		FName* Val = static_cast<FName*>(Prop.ValuePtr);
		FString Current = Val->ToString();

		if (strcmp(Prop.Name, "Particle") == 0)
		{
			if (UBillboardComponent* BillboardComp = Cast<UBillboardComponent>(SelectedComponent))
			{
				if (!Cast<USubUVComponent>(SelectedComponent))
				{
					const TArray<FString>& TexturePaths = FResourceManager::Get().GetTextureFilePath();
					if (ImGui::BeginCombo("Sprite Texture", Current.empty() ? "<None>" : Current.c_str()))
					{
						for (const FString& TexturePath : TexturePaths)
						{
							const bool bSelected = (Current == TexturePath);
							if (ImGui::Selectable(TexturePath.c_str(), bSelected))
							{
								BillboardComp->SetTextureName(TexturePath);
								bChanged = true;
							}
							if (bSelected)
							{
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}
					break;
				}
			}
		}

		// 리소스 키와 매칭되는 프로퍼티면 콤보 박스로 렌더링
		TArray<FString> Names;
		if (strcmp(Prop.Name, "Font") == 0)
			Names = FResourceManager::Get().GetFontNames();
		else if (strcmp(Prop.Name, "Particle") == 0)
			Names = FResourceManager::Get().GetParticleNames();

		if (!Names.empty())
		{
			if (ImGui::BeginCombo(Prop.Name, Current.c_str()))
			{
				for (const auto& Name : Names)
				{
					bool bSelected = (Current == Name);
					if (ImGui::Selectable(Name.c_str(), bSelected))
					{
						*Val = FName(Name);
						bChanged = true;
					}
					if (bSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}
		else
		{
			char Buf[256];
			strncpy_s(Buf, sizeof(Buf), Current.c_str(), _TRUNCATE);
			if (ImGui::InputText(Prop.Name, Buf, sizeof(Buf)))
			{
				*Val = FName(Buf);
				bChanged = true;
			}
		}
		break;
	}
	case EPropertyType::Material:
	{
		TArray<UMaterialInterface*>* Slots = static_cast<TArray<UMaterialInterface*>*>(Prop.ValuePtr);
		if (!Slots)
		{
			break;
		}
		RefreshMaterialSlotCache();

		DrawDetailsSeparator();
		DrawDetailsSectionLabel(Prop.Name);
		ImGui::PushID(Prop.Name);
		for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(Slots->size()); ++SlotIndex)
		{
			ImGui::PushID(SlotIndex);
			UMaterialInterface* CurrentMaterial = (*Slots)[SlotIndex];
			const FString CurrentLabel = CurrentMaterial
				? (CurrentMaterial->GetFilePath().empty() ? CurrentMaterial->GetName() : FPaths::Normalize(CurrentMaterial->GetFilePath()))
				: FString("None");

			ImGui::Text("Slot %d", SlotIndex);
			ImGui::SameLine();

			const float EditButtonWidth = 48.0f;
			ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x - EditButtonWidth - ImGui::GetStyle().ItemSpacing.x));
			if (ImGui::BeginCombo("##MaterialSlot", CurrentLabel.c_str()))
			{
				const bool bNoneSelected = CurrentMaterial == nullptr;
				if (ImGui::Selectable("None", bNoneSelected))
				{
					if (EditorEngine)
					{
						EditorEngine->GetUndoSystem().CaptureSnapshot("Edit Material Slot");
						bPropertyEditUndoCaptured = true;
					}
					(*Slots)[SlotIndex] = nullptr;
					bChanged = true;
				}
				if (bNoneSelected)
				{
					ImGui::SetItemDefaultFocus();
				}

				for (int32 MaterialIndex = 0; MaterialIndex < static_cast<int32>(CachedMaterialSlotNames.size()); ++MaterialIndex)
				{
					ImGui::PushID(MaterialIndex);
					UMaterialInterface* Candidate = CachedMaterialSlotMaterials[MaterialIndex];
					if (!Candidate)
					{
						ImGui::PopID();
						continue;
					}

					const FString& MaterialLabel = CachedMaterialSlotNames[MaterialIndex].empty()
						? FString("<Unnamed Material>")
						: CachedMaterialSlotNames[MaterialIndex];
					const bool bSelected = Candidate == CurrentMaterial;
					if (ImGui::Selectable(MaterialLabel.c_str(), bSelected))
					{
						if (EditorEngine)
						{
							EditorEngine->GetUndoSystem().CaptureSnapshot("Edit Material Slot");
							bPropertyEditUndoCaptured = true;
						}
						(*Slots)[SlotIndex] = Candidate;
						bChanged = true;
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
					if (ImGui::IsItemHovered())
					{
						RenderMaterialPreviewTooltip(Candidate);
					}
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			if (ImGui::IsItemHovered() && CurrentMaterial)
			{
				RenderMaterialPreviewTooltip(CurrentMaterial);
			}

			ImGui::SameLine();
			if (ImGui::Button("Edit"))
			{
				if (UPrimitiveComponent* PrimitiveComp = Cast<UPrimitiveComponent>(SelectedComponent))
				{
					EditorEngine->GetMainPanel().OpenMaterialSlot(PrimitiveComp, SlotIndex);
				}
			}
			if (ImGui::IsItemHovered())
			{
				if (CurrentMaterial)
				{
					RenderMaterialPreviewTooltip(CurrentMaterial);
				}
				else
				{
					ImGui::SetTooltip("Open this material slot in Material Editor");
				}
			}
			ImGui::PopID();
		}
		ImGui::PopID();
		break;
	}
    case EPropertyType::Enum:
	{
		int* Val = static_cast<int*>(Prop.ValuePtr);
		if (Prop.EnumNames && Prop.EnumCount)
			bChanged = ImGui::Combo(Prop.Name, Val, Prop.EnumNames, Prop.EnumCount);
		break;
	}
	case EPropertyType::Vec3Array:
	{
		TArray<FVector>* Arr = static_cast<TArray<FVector>*>(Prop.ValuePtr);
		int32 ToRemove = -1;

		ImGui::Text("%s", Prop.Name);
		ImGui::Spacing();

		for (int32 i = 0; i < static_cast<int32>(Arr->size()); i++)
		{
			ImGui::PushID(i);

			float Val[3] = { (*Arr)[i].X, (*Arr)[i].Y, (*Arr)[i].Z };
			char Label[32];
			snprintf(Label, sizeof(Label), "[%d]", i);

			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - UIConstants::XButtonSize - 8.0f);
			if (ImGui::DragFloat3(Label, Val, 1.0f))
			{
				(*Arr)[i] = FVector(Val[0], Val[1], Val[2]);
				bChanged = true;
			}

			ImGui::SameLine();
			char XId[32];
			snprintf(XId, sizeof(XId), "##rm_%d", i);
			if (DrawXButton(XId)) ToRemove = i;

			ImGui::PopID();
		}

		if (ToRemove >= 0)
		{
			if (EditorEngine)
			{
				EditorEngine->GetUndoSystem().CaptureSnapshot("Edit Property");
				bPropertyEditUndoCaptured = true;
			}
			Arr->erase(Arr->begin() + ToRemove);
			bChanged = true;
		}

		char AddLabel[64];
		snprintf(AddLabel, sizeof(AddLabel), "+ Add##%s", Prop.Name);
		if (ImGui::Button(AddLabel, ImVec2(-1, 0)))
		{
			if (EditorEngine)
			{
				EditorEngine->GetUndoSystem().CaptureSnapshot("Edit Property");
				bPropertyEditUndoCaptured = true;
			}
			Arr->push_back(Arr->empty() ? FVector(0.f, 0.f, 0.f) : Arr->back());
			bChanged = true;
		}
		break;
	}
	case EPropertyType::SRV:
	{
		ID3D11ShaderResourceView* SRV = static_cast<ID3D11ShaderResourceView*>(Prop.ValuePtr);
		if (SRV)
		{
			const FSRVDisplayInfo* Info = static_cast<const FSRVDisplayInfo*>(Prop.ExtraData);
			if (Info)
			{
				ImGui::Image(SRV,
					ImVec2(Info->ImageWidth, Info->ImageHeight),
					ImVec2(Info->UV0X, Info->UV0Y),
					ImVec2(Info->UV1X, Info->UV1Y));
			}
			else
			{
				ImGui::Image(SRV, ImVec2(256, 256));
			}
		}
		break;
	}
    case EPropertyType::CubeSRV:
    {
        auto CubeSRV = static_cast<ID3D11ShaderResourceView**>(Prop.ValuePtr);
        if (CubeSRV)
        {
            for (int i = 0; i < 6; i++)
            {
                if (!CubeSRV[i])
                {
                    continue;
                }

                ImGui::Image(CubeSRV[i], ImVec2(64, 64));

                if ((i % 3) != 2)
                {
                    ImGui::SameLine();
                }
            }
        }
        break;
    }
	}

	if (ImGui::IsItemActivated() && !bPropertyEditUndoCaptured && EditorEngine)
	{
		EditorEngine->GetUndoSystem().CaptureSnapshot("Edit Property");
		bPropertyEditUndoCaptured = true;
	}

	if (bChanged && SelectedComponent)
	{
		if (!bPropertyEditUndoCaptured && EditorEngine)
		{
			EditorEngine->GetUndoSystem().CaptureSnapshot("Edit Property");
			bPropertyEditUndoCaptured = true;
		}
		SelectedComponent->PostEditChangeProperty({ Prop.Name, EPropertyChangeType::ValueSet });
		if (EditorEngine)
		{
			EditorEngine->GetMainPanel().GetSceneWidget().MarkSceneDirty();
		}
	}

	if (ImGui::IsItemDeactivatedAfterEdit() || !ImGui::IsAnyItemActive())
	{
		bPropertyEditUndoCaptured = false;
	}
}

void FEditorPropertyWidget::RefreshMaterialSlotCache(bool bForce)
{
	if (!bForce && !CachedMaterialSlotNames.empty())
	{
		return;
	}

	CachedMaterialSlotNames.clear();
	CachedMaterialSlotMaterials.clear();

	TArray<FString> Names = FResourceManager::Get().GetMaterialInterfaceNames();
	CachedMaterialSlotNames.reserve(Names.size());
	CachedMaterialSlotMaterials.reserve(Names.size());
	for (const FString& Name : Names)
	{
		UMaterialInterface* Material = FResourceManager::Get().GetMaterialInterface(Name);
		if (!Material)
		{
			continue;
		}
		CachedMaterialSlotNames.push_back(Name);
		CachedMaterialSlotMaterials.push_back(Material);
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

	FMaterialParamValue ParamValue;
	if (Material->GetParam("DiffuseMap", ParamValue)
		&& ParamValue.Type == EMaterialParamType::Texture
		&& std::holds_alternative<UTexture*>(ParamValue.Value))
	{
		if (UTexture* Texture = std::get<UTexture*>(ParamValue.Value))
		{
			if (ID3D11ShaderResourceView* SRV = Texture->GetSRV())
			{
				ImGui::Image(reinterpret_cast<ImTextureID>(SRV), ImVec2(128.0f, 128.0f));
				ImGui::Separator();
			}
		}
	}

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
				SceneName = EditorEngine->GetMainPanel().GetSceneWidget().GetSceneName();
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
			EditorEngine->GetMainPanel().GetSceneWidget().MarkSceneDirty();
		}
	}
}
