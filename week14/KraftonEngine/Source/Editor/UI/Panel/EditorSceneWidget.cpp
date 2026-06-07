#include "Editor/UI/Panel/EditorSceneWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/UI/ContentBrowser/ContentItem.h"
#include "Editor/UI/Util/EditorFileUtils.h"
#include "Editor/Undo/EditorUndoCommand.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/Object.h"
#include "Platform/Paths.h"
#include "Render/Types/MinimalViewInfo.h"

#include "ImGui/imgui.h"
#include "Profiling/Stats/Stats.h"

#include <cstring>
#include <algorithm>
#include <filesystem>

void FEditorSceneWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
}

void FEditorSceneWidget::Render(float DeltaTime)
{
	if (!EditorEngine)
	{
		return;
	}

	(void)DeltaTime;
	ImGui::SetNextWindowSize(ImVec2(400.0f, 350.0f), ImGuiCond_Once);

	ImGui::Begin("Scene Manager");

	// 씬 파일 작업은 상단 메뉴로 옮기고, Scene Manager는 액터 목록만 유지한다.
	RenderActorOutliner();

	ImGui::End();
}

void FEditorSceneWidget::RenderActorOutliner()
{
	SCOPE_STAT_CAT("SceneWidget::ActorOutliner", "5_UI");

	UWorld* World = EditorEngine->GetWorld();
	if (!World) return;

	const TArray<AActor*>& Actors = World->GetActors();

	// null이 아닌 유효 Actor 인덱스만 수집 (Clipper는 연속 인덱스 필요)
	ValidActorIndices.clear();
	ValidActorIndices.reserve(Actors.size());
	for (int32 i = 0; i < static_cast<int32>(Actors.size()); ++i)
	{
		if (Actors[i]) ValidActorIndices.push_back(i);
	}

	// 삭제된 actor의 UUID가 그룹에 남아 있으면 표시/저장 전에 정리합니다.
	FSceneOutlinerState& OutlinerState = World->GetEditorOutlinerState();
	for (FSceneOutlinerGroup& Group : OutlinerState.Groups)
	{
		if (Group.ParentGroupId == Group.GroupId || !OutlinerState.FindGroup(Group.ParentGroupId))
		{
			Group.ParentGroupId = 0;
		}

		Group.ActorUUIDs.erase(
			std::remove_if(
				Group.ActorUUIDs.begin(),
				Group.ActorUUIDs.end(),
				[this](uint32 ActorUUID)
				{
					return ResolveActorByUUID(ActorUUID) == nullptr;
				}),
			Group.ActorUUIDs.end());
	}
	OutlinerState.RemoveEmptyGroups();

	ImGui::Text("Actors (%d)", static_cast<int32>(ValidActorIndices.size()));
	ImGui::Separator();

	RenderRenameGroupPopup();

	ImGui::BeginChild("ActorList", ImVec2(0, 0), ImGuiChildFlags_Borders);

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("PrefabContentItem"))
		{
			const FContentItem ContentItem = *reinterpret_cast<const FContentItem*>(Payload->Data);
			InstantiatePrefabFromContentItem(ContentItem);
		}
		ImGui::EndDragDropTarget();
	}

	for (FSceneOutlinerGroup& Group : OutlinerState.Groups)
	{
		if (OutlinerState.IsTopLevelGroup(Group))
		{
			RenderGroupRow(Group, Actors);
		}
	}

	for (int32 ActorIndex : ValidActorIndices)
	{
		AActor* Actor = Actors[ActorIndex];
		if (!Actor || OutlinerState.IsActorGrouped(Actor->GetUUID()))
		{
			continue;
		}

		RenderActorRow(Actor, Actors);
	}

	RenderOutlinerContextMenu();
	ImGui::EndChild();
}

void FEditorSceneWidget::RenderActorRow(AActor* Actor, const TArray<AActor*>& Actors)
{
	if (!Actor)
	{
		return;
	}

	FSelectionManager& Selection = EditorEngine->GetSelectionManager();
	const FString& StoredName = Actor->GetFName().ToString();
	const char* DisplayName = StoredName.empty()
		? Actor->GetClass()->GetName()
		: StoredName.c_str();

	const bool bIsSelected = Selection.IsSelected(Actor);
	ImGui::PushID(Actor);
	if (ImGui::Selectable(DisplayName, bIsSelected))
	{
		if (ImGui::GetIO().KeyShift)
		{
			Selection.SelectRange(Actor, Actors);
		}
		else if (ImGui::GetIO().KeyCtrl)
		{
			Selection.ToggleSelect(Actor);
		}
		else
		{
			Selection.Select(Actor);
		}
	}

	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem("Group Selected"))
		{
			GroupSelectedActors();
		}
		if (ImGui::MenuItem("Create Prefab From Selection"))
		{
			CreatePrefabFromSelectedActors();
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();
}

void FEditorSceneWidget::RenderGroupRow(FSceneOutlinerGroup& Group, const TArray<AActor*>& Actors)
{
	FSelectionManager& Selection = EditorEngine->GetSelectionManager();
	UWorld* World = EditorEngine->GetWorld();
	if (!World)
	{
		return;
	}
	FSceneOutlinerState& OutlinerState = World->GetEditorOutlinerState();
	const bool bGroupSelected = Selection.IsGroupSelected(Group.GroupId);

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (bGroupSelected)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (Group.bExpanded)
	{
		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	}

	ImGui::PushID(static_cast<int>(Group.GroupId));
	const bool bOpen = ImGui::TreeNodeEx(Group.Name.c_str(), Flags);
	Group.bExpanded = bOpen;

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		// 그룹 클릭은 actor 선택과 별도로 그룹 선택 컨텍스트를 기록합니다.
		if (ImGui::GetIO().KeyCtrl)
		{
			Selection.ToggleSelectGroup(Group.GroupId);
		}
		else
		{
			Selection.SelectGroup(Group.GroupId);
		}
	}

	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem("Rename Group"))
		{
			PendingRenameGroupId = Group.GroupId;
			strncpy_s(RenameGroupBuffer, sizeof(RenameGroupBuffer), Group.Name.c_str(), _TRUNCATE);
			ImGui::OpenPopup("##RenameSceneGroup");
		}
		if (ImGui::MenuItem("Ungroup"))
		{
			if (UWorld* World = EditorEngine->GetWorld())
			{
				World->GetEditorOutlinerState().RemoveGroup(Group.GroupId);
				Selection.ClearSelection();
			}
			ImGui::EndPopup();
			ImGui::PopID();
			if (bOpen)
			{
				ImGui::TreePop();
			}
			return;
		}
		if (ImGui::MenuItem("Create Prefab From Group"))
		{
			Selection.ClearSelection();
			TArray<uint32> ActorUUIDs;
			OutlinerState.CollectActorUUIDsRecursive(Group.GroupId, ActorUUIDs);
			for (uint32 ActorUUID : ActorUUIDs)
			{
				if (AActor* Actor = ResolveActorByUUID(ActorUUID))
				{
					Selection.ToggleSelect(Actor);
				}
			}
			CreatePrefabFromSelectedActors();
		}
		ImGui::EndPopup();
	}

	if (bOpen)
	{
		TArray<uint32> ChildGroupIds;
		OutlinerState.CollectChildGroupIds(Group.GroupId, ChildGroupIds);
		for (uint32 ChildGroupId : ChildGroupIds)
		{
			if (FSceneOutlinerGroup* ChildGroup = OutlinerState.FindGroup(ChildGroupId))
			{
				RenderGroupRow(*ChildGroup, Actors);
			}
		}

		for (uint32 ActorUUID : Group.ActorUUIDs)
		{
			if (AActor* Actor = ResolveActorByUUID(ActorUUID))
			{
				RenderActorRow(Actor, Actors);
			}
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

void FEditorSceneWidget::RenderOutlinerContextMenu()
{
	if (ImGui::BeginPopupContextWindow("##SceneManagerContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		const bool bHasSelection = EditorEngine && !EditorEngine->GetSelectionManager().IsEmpty();
		if (!bHasSelection)
		{
			ImGui::BeginDisabled();
		}
		if (ImGui::MenuItem("Group Selected"))
		{
			GroupSelectedActors();
		}
		if (ImGui::MenuItem("Create Prefab From Selection"))
		{
			CreatePrefabFromSelectedActors();
		}
		if (!bHasSelection)
		{
			ImGui::EndDisabled();
		}
		ImGui::EndPopup();
	}
}

void FEditorSceneWidget::RenderRenameGroupPopup()
{
	if (PendingRenameGroupId == 0)
	{
		return;
	}

	ImGui::OpenPopup("##RenameSceneGroup");
	if (ImGui::BeginPopupModal("##RenameSceneGroup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Rename Group");
		ImGui::Separator();
		if (ImGui::IsWindowAppearing())
		{
			ImGui::SetKeyboardFocusHere();
		}

		const bool bSubmit = ImGui::InputText(
			"##group-name",
			RenameGroupBuffer,
			sizeof(RenameGroupBuffer),
			ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

		const bool bOk = bSubmit || ImGui::Button("OK");
		ImGui::SameLine();
		const bool bCancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);

		if (bOk)
		{
			if (UWorld* World = EditorEngine->GetWorld())
			{
				if (FSceneOutlinerGroup* Group = World->GetEditorOutlinerState().FindGroup(PendingRenameGroupId))
				{
					Group->Name = RenameGroupBuffer[0] != '\0'
						? FString(RenameGroupBuffer)
						: FSceneOutlinerState::MakeDefaultGroupName(Group->GroupId);
				}
			}
			PendingRenameGroupId = 0;
			ImGui::CloseCurrentPopup();
		}
		if (bCancel)
		{
			PendingRenameGroupId = 0;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void FEditorSceneWidget::GroupSelectedActors()
{
	if (!EditorEngine)
	{
		return;
	}

	UWorld* World = EditorEngine->GetWorld();
	if (!World)
	{
		return;
	}

	FSelectionManager& Selection = EditorEngine->GetSelectionManager();
	TArray<uint32> ActorUUIDs = Selection.GetDirectSelectedActorUUIDs();
	FSceneOutlinerState& OutlinerState = World->GetEditorOutlinerState();
	TArray<uint32> GroupIds = OutlinerState.MakeRootGroupSelection(Selection.GetSelectedGroupIds());
	TArray<uint32> GroupActorUUIDs = OutlinerState.GetActorUUIDsForGroups(GroupIds);
	ActorUUIDs.erase(
		std::remove_if(
			ActorUUIDs.begin(),
			ActorUUIDs.end(),
			[&GroupActorUUIDs](uint32 ActorUUID)
			{
				return std::find(GroupActorUUIDs.begin(), GroupActorUUIDs.end(), ActorUUID) != GroupActorUUIDs.end();
			}),
		ActorUUIDs.end());
	if (ActorUUIDs.empty() && GroupIds.empty())
	{
		return;
	}

	const uint32 NewGroupId = OutlinerState.CreateGroup("", ActorUUIDs, GroupIds);
	if (NewGroupId != 0)
	{
		Selection.SelectGroup(NewGroupId);
	}
}

void FEditorSceneWidget::CreatePrefabFromSelectedActors()
{
	if (!EditorEngine)
	{
		return;
	}

	UWorld* World = EditorEngine->GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> SelectedActors = GetSelectedActors();
	if (SelectedActors.empty())
	{
		return;
	}

	const FString SelectedPath = FEditorFileUtils::SaveFileDialog({
		.Filter = L"Prefab Files (*.prefab)\0*.prefab\0All Files (*.*)\0*.*\0",
		.Title = L"Create Prefab",
		.DefaultExtension = L"prefab",
		.InitialDirectory = FPaths::AssetDir().c_str(),
		.DefaultFileName = L"NewPrefab.prefab",
		.OwnerWindowHandle = nullptr,
		.bFileMustExist = false,
		.bPathMustExist = true,
		.bPromptOverwrite = true,
		.bReturnRelativeToProjectRoot = false,
	});
	if (SelectedPath.empty())
	{
		return;
	}

	if (FSceneSaveManager::SavePrefabAsJSON(SelectedPath, SelectedActors, World->GetEditorOutlinerState()))
	{
		EditorEngine->RefreshContentBrowser();
	}
}

void FEditorSceneWidget::InstantiatePrefabFromContentItem(const FContentItem& ContentItem)
{
	if (!EditorEngine)
	{
		return;
	}

	UWorld* World = EditorEngine->GetWorld();
	if (!World)
	{
		return;
	}

	const FEditorSelectionSnapshot SelectionBefore = CaptureEditorSelection(&EditorEngine->GetSelectionManager());
	TArray<AActor*> CreatedActors;
	const FString FilePath = FPaths::ToUtf8(ContentItem.Path.wstring());
	if (!FSceneSaveManager::InstantiatePrefabFromJSON(
			FilePath,
			World,
			GetDefaultPrefabPlacementLocation(),
			CreatedActors,
			World->GetEditorOutlinerState()))
	{
		return;
	}

	FSelectionManager& Selection = EditorEngine->GetSelectionManager();
	Selection.ClearSelection();
	for (AActor* Actor : CreatedActors)
	{
		Selection.ToggleSelect(Actor);
	}

	EditorEngine->PushExecutedUndoCommand(MakeActorCreateUndoCommand(
		CreatedActors,
		SelectionBefore,
		CaptureEditorSelection(&Selection),
		"Place Prefab"));
}

AActor* FEditorSceneWidget::ResolveActorByUUID(uint32 ActorUUID) const
{
	if (!EditorEngine || ActorUUID == 0)
	{
		return nullptr;
	}

	UWorld* World = EditorEngine->GetWorld();
	AActor* Actor = Cast<AActor>(UObjectManager::Get().FindByUUID(ActorUUID));
	return Actor && Actor->GetWorld() == World ? Actor : nullptr;
}

TArray<AActor*> FEditorSceneWidget::GetSelectedActors() const
{
	TArray<AActor*> Result;
	if (!EditorEngine)
	{
		return Result;
	}

	for (AActor* Actor : EditorEngine->GetSelectionManager().GetSelectedActors())
	{
		if (Actor)
		{
			Result.push_back(Actor);
		}
	}
	return Result;
}

FVector FEditorSceneWidget::GetDefaultPrefabPlacementLocation() const
{
	if (!EditorEngine)
	{
		return FVector::ZeroVector;
	}

	FMinimalViewInfo POV;
	if (EditorEngine->GetActiveViewportPOV(POV))
	{
		return POV.Location + POV.Rotation.GetForwardVector() * 10.0f;
	}
	return FVector::ZeroVector;
}
