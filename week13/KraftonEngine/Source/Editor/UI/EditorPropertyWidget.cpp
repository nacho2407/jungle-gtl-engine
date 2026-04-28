#include "Editor/UI/EditorPropertyWidget.h"

#include "Editor/EditorEngine.h"

#include "ImGui/imgui.h"
#include "Component/ActorComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/MeshComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SceneComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "Component/DecalComponent.h"
#include "Component/HeightFogComponent.h"
#include "Core/PropertyTypes.h"
#include "Core/ClassTypes.h"
#include "Resource/ResourceManager.h"
#include "Object/FName.h"
#include "Object/ObjectIterator.h"
#include "Materials/Material.h"
#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Platform/Paths.h"

#include <Windows.h>
#include <commdlg.h>
#include <algorithm>
#include <array>
#include <cfloat>
#include <cstring>
#include <filesystem>

#include "Materials/MaterialManager.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

namespace
{
	bool ShouldHideInComponentTree(const UActorComponent* Component, bool bShowEditorOnlyComponents)
	{
		if (!Component)
		{
			return true;
		}

		return Component->IsHiddenInComponentTree()
			&& !(bShowEditorOnlyComponents && Component->IsEditorOnlyComponent());
	}

	struct FComponentClassGroup
	{
		const char* Label = nullptr;
		UClass* AnchorClass = nullptr;
		TArray<UClass*> Classes;
	};

	void AddComponentClassGroup(TArray<FComponentClassGroup>& Groups, const char* Label, UClass* AnchorClass)
	{
		FComponentClassGroup Group;
		Group.Label = Label;
		Group.AnchorClass = AnchorClass;
		Groups.push_back(Group);
	}

	UClass* FindComponentClassGroupAnchor(UClass* ComponentClass, const TArray<FComponentClassGroup>& Groups)
	{
		if (!ComponentClass)
		{
			return nullptr;
		}

		// UTextRenderComponent는 C++ 상속은 Billboard지만 RTTI 등록 부모가 Primitive라서 명시적으로 묶는다.
		if (ComponentClass == UTextRenderComponent::StaticClass())
		{
			return UBillboardComponent::StaticClass();
		}

		for (const FComponentClassGroup& Group : Groups)
		{
			if (Group.AnchorClass && ComponentClass->IsA(Group.AnchorClass))
			{
				return Group.AnchorClass;
			}
		}

		return nullptr;
	}
}

static FString RemoveExtension(const FString& Path)
{
	size_t DotPos = Path.find_last_of('.');
	if (DotPos == FString::npos)
	{
		return Path;
	}
	return Path.substr(0, DotPos);
}

static FString GetStemFromPath(const FString& Path)
{
	size_t SlashPos = Path.find_last_of("/\\");
	FString FileName = (SlashPos == FString::npos) ? Path : Path.substr(SlashPos + 1);
	return RemoveExtension(FileName);
}

FString FEditorPropertyWidget::OpenObjFileDialog()
{
	wchar_t FilePath[MAX_PATH] = {};

	OPENFILENAMEW Ofn = {};
	Ofn.lStructSize = sizeof(Ofn);
	Ofn.hwndOwner = nullptr;
	Ofn.lpstrFilter = L"OBJ Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
	Ofn.lpstrFile = FilePath;
	Ofn.nMaxFile = MAX_PATH;
	Ofn.lpstrTitle = L"Import OBJ Mesh";
	Ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (GetOpenFileNameW(&Ofn))
	{
		std::filesystem::path AbsPath = std::filesystem::path(FilePath).lexically_normal();
		std::filesystem::path RootPath = std::filesystem::path(FPaths::RootDir());
		std::filesystem::path RelPath = AbsPath.lexically_relative(RootPath);

		// 상대 경로 변환 실패 시 (드라이브가 다른 경우 등) 절대 경로를 그대로 반환
		if (RelPath.empty() || RelPath.wstring().starts_with(L".."))
		{
			return FPaths::ToUtf8(AbsPath.generic_wstring());
		}
		return FPaths::ToUtf8(RelPath.generic_wstring());
	}

	return FString();
}

void FEditorPropertyWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	ImGui::SetNextWindowSize(ImVec2(350.0f, 500.0f), ImGuiCond_Once);

	ImGui::Begin("Property Window");

	FSelectionManager& Selection = EditorEngine->GetSelectionManager();
	AActor* PrimaryActor = Selection.GetPrimarySelection();
	if (!PrimaryActor)
	{
		SelectedComponent = nullptr;
		LastSelectedActor = nullptr;
		bActorSelected = true;
		ImGui::Text("No object selected.");
		ImGui::End();
		return;
	}

	// Actor 선택이 바뀌면 초기화
	if (PrimaryActor != LastSelectedActor)
	{
		SelectedComponent = nullptr;
		LastSelectedActor = PrimaryActor;
		bActorSelected = true;
	}

	const TArray<AActor*>& SelectedActors = Selection.GetSelectedActors();
	const int32 SelectionCount = static_cast<int32>(SelectedActors.size());

	// ========== 고정 영역: Actor Info (clickable) ==========
	if (SelectionCount > 1)
	{
		ImGui::Text("Class: %s", PrimaryActor->GetClass()->GetName());

		FString PrimaryName = PrimaryActor->GetFName().ToString();
		if (PrimaryName.empty()) PrimaryName = PrimaryActor->GetClass()->GetName();

		bool bHighlight = bActorSelected;
		if (bHighlight) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
		ImGui::Text("Name: %s (+%d)", PrimaryName.c_str(), SelectionCount - 1);
		if (bHighlight) ImGui::PopStyleColor();
		if (ImGui::IsItemClicked())
		{
			bActorSelected = true;
			SelectedComponent = nullptr;
		}
		ImGui::SameLine();
		char RemoveLabel[64];
		snprintf(RemoveLabel, sizeof(RemoveLabel), "Remove %d Objects", SelectionCount);
		if (ImGui::Button(RemoveLabel))
		{
			// 선택 해제를 먼저 수행 (dangling pointer로 Proxy 접근 방지)
			TArray<AActor*> ToDelete(SelectedActors.begin(), SelectedActors.end());
			Selection.ClearSelection();
			for (AActor* Actor : ToDelete)
			{
				if (Actor && Actor->GetWorld())
				{
					Actor->GetWorld()->DestroyActor(Actor);
				}
			}
			// GPU Occlusion staging에 남은 dangling proxy 포인터 무효화
			EditorEngine->InvalidateOcclusionResults();
			SelectedComponent = nullptr;
			LastSelectedActor = nullptr;
			ImGui::End();
			return;
		}
	}
	else
	{
		ImGui::Text("Class: %s", PrimaryActor->GetClass()->GetName());

		// Actor 이름: 클릭 가능, 선택 시 하이라이트
		bool bHighlight = bActorSelected;
		if (bHighlight) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
		ImGui::Text("Name: %s", PrimaryActor->GetFName().ToString().c_str());
		if (bHighlight) ImGui::PopStyleColor();
		if (ImGui::IsItemClicked())
		{
			bActorSelected = true;
			SelectedComponent = nullptr;
		}
		ImGui::SameLine();
		if (ImGui::Button("Remove"))
		{
			// 선택 해제를 먼저 수행 (dangling pointer로 Proxy 접근 방지)
			AActor* ToDelete = PrimaryActor;
			Selection.ClearSelection();
			if (ToDelete && ToDelete->GetWorld())
			{
				ToDelete->GetWorld()->DestroyActor(ToDelete);
			}
			// GPU Occlusion staging에 남은 dangling proxy 포인터 무효화
			EditorEngine->InvalidateOcclusionResults();
			SelectedComponent = nullptr;
			LastSelectedActor = nullptr;
			ImGui::End();
			return;
		}
	}

	// ========== 고정 영역: Component Tree ==========
	SEPARATOR();
	ImGui::Checkbox("Show Editor Only", &bShowEditorOnlyComponents);
	ImGui::Separator();
	RenderComponentTree(PrimaryActor);

	// ========== 스크롤 영역: Details ==========
	SEPARATOR();
	ImGui::Text("Details");
	ImGui::Separator();

	float ScrollHeight = ImGui::GetContentRegionAvail().y;
	if (ScrollHeight < 50.0f) ScrollHeight = 50.0f;

	ImGui::BeginChild("##Details", ImVec2(0, ScrollHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	{
		RenderDetails(PrimaryActor, SelectedActors);
	}
	ImGui::EndChild();

	ImGui::End();
}

void FEditorPropertyWidget::RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
	if (bActorSelected)
	{
		RenderActorProperties(PrimaryActor, SelectedActors);
	}
	else if (SelectedComponent && SelectedActors.size() >= 2)
	{
		// 다중 선택 시 모든 액터의 타입이 동일한지 검증
		UClass* PrimaryClass = PrimaryActor->GetClass();
		bool bAllSameType = true;
		for (const AActor* Actor : SelectedActors)
		{
			if (Actor && Actor->GetClass() != PrimaryClass)
			{
				bAllSameType = false;
				break;
			}
		}

		if (!bAllSameType)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Multi-edit unavailable");
			ImGui::TextWrapped(
				"Selected actors have different types. "
				"Multi-component editing requires all selected actors to be the same type.");

			ImGui::Spacing();
			ImGui::TextDisabled("Primary: %s", PrimaryClass->GetName());
			for (const AActor* Actor : SelectedActors)
			{
				if (Actor && Actor->GetClass() != PrimaryClass)
				{
					ImGui::TextDisabled("  Mismatch: %s (%s)",
						Actor->GetFName().ToString().c_str(),
						Actor->GetClass()->GetName());
				}
			}
		}
		else
		{
			RenderComponentProperties(PrimaryActor, SelectedActors);
		}
	}
	else if (SelectedComponent)
	{
		RenderComponentProperties(PrimaryActor, SelectedActors);
	}
	else
	{
		ImGui::TextDisabled("Select an actor or component to view details.");
	}
}

void FEditorPropertyWidget::RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
	ImGui::Text("Actor: %s", PrimaryActor->GetClass()->GetName());
	ImGui::Text("Name: %s", PrimaryActor->GetFName().ToString().c_str());

	if (PrimaryActor->GetRootComponent())
	{
		ImGui::Separator();
		ImGui::Text("Transform");
		ImGui::Spacing();

		FVector Pos = PrimaryActor->GetActorLocation();
		float PosArray[3] = { Pos.X, Pos.Y, Pos.Z };

		USceneComponent* RootComp = PrimaryActor->GetRootComponent();

		FVector Scale = PrimaryActor->GetActorScale();
		float ScaleArray[3] = { Scale.X, Scale.Y, Scale.Z };

		if (ImGui::DragFloat3("Location", PosArray, 0.1f))
		{
			FVector Delta = FVector(PosArray[0], PosArray[1], PosArray[2]) - Pos;
			for (AActor* Actor : SelectedActors)
			{
				if (Actor) Actor->AddActorWorldOffset(Delta);
			}
			EditorEngine->GetGizmo()->UpdateGizmoTransform();
		}
		{
			// Rotation: CachedEditRotator를 X=Roll(X축), Y=Pitch(Y축), Z=Yaw(Z축)로 노출
			FRotator& CachedRot = RootComp->GetCachedEditRotator();
			FRotator PrevRot = CachedRot;
			float RotXYZ[3] = { CachedRot.Roll, CachedRot.Pitch, CachedRot.Yaw };

			if (ImGui::DragFloat3("Rotation", RotXYZ, 0.1f))
			{
				CachedRot.Roll = RotXYZ[0];
				CachedRot.Pitch = RotXYZ[1];
				CachedRot.Yaw = RotXYZ[2];

				if (SelectedActors.size() > 1)
				{
					FRotator Delta = CachedRot - PrevRot;
					for (AActor* Actor : SelectedActors)
					{
						if (!Actor || Actor == PrimaryActor) continue;
						USceneComponent* Root = Actor->GetRootComponent();
						if (Root)
						{
							FRotator Other = Root->GetCachedEditRotator();
							Root->SetRelativeRotation(Other + Delta);
						}
					}
				}
				RootComp->ApplyCachedEditRotator();
				EditorEngine->GetGizmo()->UpdateGizmoTransform();
			}
		}
		if (ImGui::DragFloat3("Scale", ScaleArray, 0.1f))
		{
			FVector Delta = FVector(ScaleArray[0], ScaleArray[1], ScaleArray[2]) - Scale;
			for (AActor* Actor : SelectedActors)
			{
				if (Actor) Actor->SetActorScale(Actor->GetActorScale() + Delta);
			}
		}


	}

	ImGui::Separator();
	bool bVisible = PrimaryActor->IsVisible();
	if (ImGui::Checkbox("Visible", &bVisible))
	{
		PrimaryActor->SetVisible(bVisible);
	}

}

void FEditorPropertyWidget::RenderComponentTree(AActor* Actor)
{
	ImGui::Text("Components");

	if (SelectedComponent && ShouldHideInComponentTree(SelectedComponent, bShowEditorOnlyComponents))
	{
		SelectedComponent = nullptr;
		bActorSelected = true;
	}

	// Get All Component Classes
	TArray<UClass*>& AllClasses = UClass::GetAllClasses();

	TArray<UClass*> ComponentClasses;
	for (UClass* Cls : AllClasses)
	{
		if (Cls->IsA(UActorComponent::StaticClass()) && !Cls->HasAnyClassFlags(CF_HiddenInComponentList))
			ComponentClasses.push_back(Cls);
	}

	std::sort(ComponentClasses.begin(), ComponentClasses.end(),
		[](const UClass* A, const UClass* B)
		{
			return strcmp(A->GetName(), B->GetName()) < 0;
		});

	//아래 클래스들로 컴포넌트 리스트를 분류합니다.
	TArray<FComponentClassGroup> ComponentGroups;
	AddComponentClassGroup(ComponentGroups, "Light", ULightComponentBase::StaticClass());
	AddComponentClassGroup(ComponentGroups, "Movement", UMovementComponent::StaticClass());
	//AddComponentClassGroup(ComponentGroups, "UBillboardComponent", UBillboardComponent::StaticClass());
	//AddComponentClassGroup(ComponentGroups, "UMeshComponent", UMeshComponent::StaticClass());
	AddComponentClassGroup(ComponentGroups, "Primitive", UPrimitiveComponent::StaticClass());
	//AddComponentClassGroup(ComponentGroups, "USceneComponent", USceneComponent::StaticClass());
	//AddComponentClassGroup(ComponentGroups, "UActorComponent", UActorComponent::StaticClass());

	TArray<UClass*> OtherClasses;
	for (UClass* Cls : ComponentClasses)
	{
		UClass* AnchorClass = FindComponentClassGroupAnchor(Cls, ComponentGroups);
		if (!AnchorClass)
		{
			OtherClasses.push_back(Cls);
			continue;
		}

		for (FComponentClassGroup& Group : ComponentGroups)
		{
			if (Group.AnchorClass == AnchorClass)
			{
				Group.Classes.push_back(Cls);
				break;
			}
		}
	}

	for (FComponentClassGroup& Group : ComponentGroups)
	{
		std::sort(Group.Classes.begin(), Group.Classes.end(),
			[](const UClass* A, const UClass* B)
			{
				return strcmp(A->GetName(), B->GetName()) < 0;
			});
	}
	std::sort(OtherClasses.begin(), OtherClasses.end(),
		[](const UClass* A, const UClass* B)
		{
			return strcmp(A->GetName(), B->GetName()) < 0;
		});

	static UClass* SelectedClass = nullptr;
	auto IsCurrentSelectionValid = [&]()
	{
		for (UClass* Cls : ComponentClasses)
		{
			if (Cls == SelectedClass)
			{
				return true;
			}
		}
		return false;
	};

	if (ComponentClasses.empty())
	{
		SelectedClass = nullptr;
	}
	else if (!IsCurrentSelectionValid())
	{
		SelectedClass = ComponentClasses.front();
	}
	const char* Preview = SelectedClass ? SelectedClass->GetName() : "None";

	const ImGuiStyle& Style = ImGui::GetStyle();
	const float ComboWidth = ImGui::GetContentRegionAvail().x;
	const float ComboHeight = ImGui::GetFrameHeight();
	const ImVec2 ComboButtonSize(ComboWidth, ComboHeight);
	if (ImGui::InvisibleButton("##ComponentClassButton", ComboButtonSize))
	{
		ImGui::OpenPopup("##ComponentClassPopup");
	}

	const ImVec2 ComboMin = ImGui::GetItemRectMin();
	const ImVec2 ComboMax = ImGui::GetItemRectMax();
	const bool bHovered = ImGui::IsItemHovered();
	const bool bPopupOpen = ImGui::IsPopupOpen("##ComponentClassPopup");
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const ImU32 FrameColor = ImGui::GetColorU32((bHovered || bPopupOpen) ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
	const ImU32 BorderColor = ImGui::GetColorU32(ImGuiCol_Border);
	const ImU32 TextColor = ImGui::GetColorU32(ImGuiCol_Text);
	const float ArrowWidth = ComboHeight;
	const ImVec2 ArrowMin(ComboMax.x - ArrowWidth, ComboMin.y);
	const ImVec2 ArrowMax(ComboMax.x, ComboMax.y);
	const float Rounding = Style.FrameRounding;

	DrawList->AddRectFilled(ComboMin, ComboMax, FrameColor, Rounding);
	DrawList->AddRect(ComboMin, ComboMax, BorderColor, Rounding);
	DrawList->AddLine(ImVec2(ArrowMin.x, ArrowMin.y), ImVec2(ArrowMin.x, ArrowMax.y), BorderColor);

	const float ArrowCenterX = (ArrowMin.x + ArrowMax.x) * 0.5f;
	const float ArrowCenterY = (ArrowMin.y + ArrowMax.y) * 0.5f;
	DrawList->AddTriangleFilled(
		ImVec2(ArrowCenterX - 4.0f, ArrowCenterY - 2.0f),
		ImVec2(ArrowCenterX + 4.0f, ArrowCenterY - 2.0f),
		ImVec2(ArrowCenterX, ArrowCenterY + 3.0f),
		TextColor);

	const ImVec2 TextMin(ComboMin.x + Style.FramePadding.x, ComboMin.y + Style.FramePadding.y);
	const ImVec2 TextMax(ArrowMin.x - Style.FramePadding.x, ComboMax.y - Style.FramePadding.y);
	DrawList->PushClipRect(TextMin, TextMax, true);
	DrawList->AddText(TextMin, TextColor, Preview);
	DrawList->PopClipRect();

	ImGui::SetNextWindowPos(ComboMin, ImGuiCond_Appearing, ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(220.0f, 320.0f), ImVec2(FLT_MAX, 520.0f));
	if (ImGui::BeginPopup("##ComponentClassPopup"))
	{
		auto RenderClassItem = [&](UClass* Cls)
		{
			bool bSelected = (SelectedClass == Cls);
			if (ImGui::Selectable(Cls->GetName(), bSelected))
			{
				SelectedClass = Cls;
				ImGui::CloseCurrentPopup();
			}
			if (bSelected)
				ImGui::SetItemDefaultFocus();
		};

		for (const FComponentClassGroup& Group : ComponentGroups)
		{
			if (Group.Classes.empty())
			{
				continue;
			}

			FString SeparatorLabel = "----";
			SeparatorLabel += Group.Label;
			SeparatorLabel += "----";
			ImGui::TextDisabled("%s", SeparatorLabel.c_str());

			for (UClass* Cls : Group.Classes)
			{
				RenderClassItem(Cls);
			}
		}

		if (!OtherClasses.empty())
		{
			ImGui::TextDisabled("----Other----");
			for (UClass* Cls : OtherClasses)
			{
				RenderClassItem(Cls);
			}
		}
		ImGui::EndPopup();
	}

	USceneComponent* Root = Actor->GetRootComponent();

	// Add Component
	if (SelectedClass && ImGui::Button("Add"))
	{
		UActorComponent* Comp = Actor->AddComponentByClass(SelectedClass);
		if (!Comp)
		{
			return;
		}

		if (SelectedClass->IsA(USceneComponent::StaticClass()))
		{
			if (SelectedComponent != nullptr && SelectedComponent->GetClass()->IsA(USceneComponent::StaticClass()))
				Cast<USceneComponent>(Comp)->AttachToComponent(Cast<USceneComponent>(SelectedComponent));
			else
				Cast<USceneComponent>(Comp)->AttachToComponent(Root);

			// 빌보드가 필요한 컴포넌트들에 대해 빌보드 생성 보장
			if (Comp->IsA<ULightComponentBase>())
			{
				Cast<ULightComponentBase>(Comp)->EnsureEditorBillboard();
			}
			else if (Comp->IsA<UDecalComponent>())
			{
				Cast<UDecalComponent>(Comp)->EnsureEditorBillboard();
			}
			else if (Comp->IsA<UHeightFogComponent>())
			{
				Cast<UHeightFogComponent>(Comp)->EnsureEditorBillboard();
			}
		}
	}

	ImGui::Separator();

	if (Root)
	{
		RenderSceneComponentNode(Root);
	}

	// Non-scene ActorComponents
	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (!Comp) continue;
		if (Comp->IsA<USceneComponent>()) continue;
		if (ShouldHideInComponentTree(Comp, bShowEditorOnlyComponents)) continue;

		FString Name = Comp->GetFName().ToString();
		const FString TypeName = Comp->GetClass()->GetName();
		const FString DefaultNamePrefix = TypeName + "_";
		const bool bUseTypeAsLabel = Name.empty()
			|| Name == TypeName
			|| Name.rfind(DefaultNamePrefix, 0) == 0;
		const char* Label = bUseTypeAsLabel ? TypeName.c_str() : Name.c_str();

		ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (!bActorSelected && SelectedComponent == Comp)
			Flags |= ImGuiTreeNodeFlags_Selected;

		ImGui::TreeNodeEx(Comp, Flags, "%s", Label);
		if (ImGui::IsItemClicked())
		{
			SelectedComponent = Comp;
			bActorSelected = false;
		}
	}
}

void FEditorPropertyWidget::RenderSceneComponentNode(USceneComponent* Comp)
{
	if (!Comp) return;
	if (ShouldHideInComponentTree(Comp, bShowEditorOnlyComponents)) return;

	FString Name = Comp->GetFName().ToString();
	if (Name.empty()) Name = Comp->GetClass()->GetName();

	const auto& Children = Comp->GetChildren();
	bool bHasVisibleChildren = false;
	for (USceneComponent* Child : Children)
	{
		if (Child && !ShouldHideInComponentTree(Child, bShowEditorOnlyComponents))
		{
			bHasVisibleChildren = true;
			break;
		}
	}

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
	if (!bHasVisibleChildren)
		Flags |= ImGuiTreeNodeFlags_Leaf;
	if (!bActorSelected && SelectedComponent == Comp)
		Flags |= ImGuiTreeNodeFlags_Selected;

	bool bIsRoot = (Comp->GetParent() == nullptr);
	bool bOpen = ImGui::TreeNodeEx(
		Comp, Flags, "%s%s (%s)",
		bIsRoot ? "[Root] " : "",
		Name.c_str(),
		Comp->GetClass()->GetName()
	);

	if (ImGui::IsItemClicked())
	{
		SelectedComponent = Comp;
		bActorSelected = false;
		EditorEngine->GetSelectionManager().SelectComponent(Comp);
	}

	// 컴포넌트 트리에서 간단하게 드래그 앤 드랍으로 부모-자식 관계 변경 가능하도록 지원
	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload("SCENE_COMPONENT_REPARENT", &Comp, sizeof(USceneComponent*));
		ImGui::Text("Reparent %s", Name.c_str());
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_COMPONENT_REPARENT"))
		{
			USceneComponent* DraggedComp = *(USceneComponent**)payload->Data;
			if (DraggedComp && DraggedComp != Comp)
			{
				// Circular dependency check: Ensure Comp is not a child of DraggedComp
				bool bIsChildOfDragged = false;
				USceneComponent* Check = Comp;
				while (Check)
				{
					if (Check == DraggedComp)
					{
						bIsChildOfDragged = true;
						break;
					}
					Check = Check->GetParent();
				}

				if (!bIsChildOfDragged)
				{
					DraggedComp->SetParent(Comp);
					if (EditorEngine && EditorEngine->GetGizmo())
					{
						EditorEngine->GetGizmo()->UpdateGizmoTransform();
					}
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (bOpen)
	{
		for (USceneComponent* Child : Children)
		{
			RenderSceneComponentNode(Child);
		}
		ImGui::TreePop();
	}
}

void FEditorPropertyWidget::RenderComponentProperties(AActor* Actor, const TArray<AActor*>& SelectedActors)
{
	ImGui::Text("Component: %s", SelectedComponent->GetClass()->GetName());
	ImGui::Text("Name: %s", SelectedComponent->GetFName().ToString().c_str());
	ImGui::SameLine();
	if (SelectedComponent != Actor->GetRootComponent())
	{
		if (ImGui::Button("Remove"))
		{
			if (SelectedComponent != nullptr)
			{
				Actor->RemoveComponent(SelectedComponent);
				SelectedComponent = nullptr;
				return;
			}
		}
	}

	ImGui::Separator();

	// PropertyDescriptor 기반 자동 위젯 렌더링
	TArray<FPropertyDescriptor> Props;
	SelectedComponent->GetEditableProperties(Props);

	bool bIsRoot = false;
	if (SelectedComponent->IsA<USceneComponent>())
	{
		USceneComponent* SceneComp = static_cast<USceneComponent*>(SelectedComponent);
		bIsRoot = (SceneComp->GetParent() == nullptr);
	}

	// Transform 프로퍼티 이름 목록
	auto IsTransformProp = [](const FString& Name) {
		return Name == "Location"
			|| Name == "Rotation"
			|| Name == "Scale";
		};

	bool bAnyChanged = false;

	// Pass 1: Transform 프로퍼티 먼저 (Root가 아닐 때만)
	if (!bIsRoot)
	{
		for (int32 i = 0; i < (int32)Props.size(); ++i)
		{
			if (IsTransformProp(Props[i].Name))
			{
				if (RenderPropertyWidget(Props, i))
				{
					bAnyChanged = true;
					PropagatePropertyChange(Props[i].Name, SelectedActors);
				}
			}
		}
		ImGui::Separator();
	}

	// Pass 2: 나머지 프로퍼티
	for (int32 i = 0; i < (int32)Props.size(); ++i)
	{
		if (IsTransformProp(Props[i].Name))
			continue;

		bool bChanged = RenderPropertyWidget(Props, i);
		if (bChanged)
		{
			bAnyChanged = true;
			PropagatePropertyChange(Props[i].Name, SelectedActors);

			if (Props[i].Type == EPropertyType::StaticMeshRef)
				break;
		}
	}

	// 실제 변경이 있었을 때만 Transform dirty 마킹
	if (bAnyChanged && SelectedComponent->IsA<USceneComponent>())
	{
		static_cast<USceneComponent*>(SelectedComponent)->MarkTransformDirty();
	}
}

void FEditorPropertyWidget::PropagatePropertyChange(const FString& PropName, const TArray<AActor*>& SelectedActors)
{
	if (!SelectedComponent || SelectedActors.size() < 2) return;

	UClass* CompClass = SelectedComponent->GetClass();
	AActor* PrimaryActor = SelectedActors[0];

	// Primary 컴포넌트에서 변경된 프로퍼티의 값 포인터 찾기
	TArray<FPropertyDescriptor> SrcProps;
	SelectedComponent->GetEditableProperties(SrcProps);

	const FPropertyDescriptor* SrcProp = nullptr;
	for (const auto& P : SrcProps)
	{
		if (P.Name == PropName) { SrcProp = &P; break; }
	}
	if (!SrcProp) return;

	for (AActor* Actor : SelectedActors)
	{
		if (!Actor || Actor == PrimaryActor) continue;

		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (!Comp || Comp->GetClass() != CompClass) continue;

			TArray<FPropertyDescriptor> DstProps;
			Comp->GetEditableProperties(DstProps);

			for (const auto& DstProp : DstProps)
			{
				if (DstProp.Name != PropName || DstProp.Type != SrcProp->Type) continue;

				size_t Size = 0;
				switch (DstProp.Type)
				{
				case EPropertyType::Bool:          Size = sizeof(bool); break;
				case EPropertyType::ByteBool:       Size = sizeof(uint8); break;
				case EPropertyType::Int:            Size = sizeof(int32); break;
				case EPropertyType::Float:          Size = sizeof(float); break;
				case EPropertyType::Vec3:
				case EPropertyType::Rotator:        Size = sizeof(float) * 3; break;
				case EPropertyType::Vec4:
				case EPropertyType::Color4:         Size = sizeof(float) * 4; break;
				case EPropertyType::String:
				case EPropertyType::SceneComponentRef:
				case EPropertyType::StaticMeshRef:  *static_cast<FString*>(DstProp.ValuePtr) = *static_cast<FString*>(SrcProp->ValuePtr); break;
				case EPropertyType::Name:           *static_cast<FName*>(DstProp.ValuePtr) = *static_cast<FName*>(SrcProp->ValuePtr); break;
				case EPropertyType::MaterialSlot:   *static_cast<FMaterialSlot*>(DstProp.ValuePtr) = *static_cast<FMaterialSlot*>(SrcProp->ValuePtr); break;
				}
				if (Size > 0)
					memcpy(DstProp.ValuePtr, SrcProp->ValuePtr, Size);

				Comp->PostEditProperty(PropName.c_str());
				break;
			}
			break; // 같은 타입의 첫 번째 컴포넌트에만 전파
		}
	}
}

bool FEditorPropertyWidget::RenderPropertyWidget(TArray<FPropertyDescriptor>& Props, int32& Index)
{
	ImGui::PushID(Index);
	FPropertyDescriptor& Prop = Props[Index];
	bool bChanged = false;

	switch (Prop.Type)
	{
	case EPropertyType::Bool:
	{
		bool* Val = static_cast<bool*>(Prop.ValuePtr);
		bChanged = ImGui::Checkbox(Prop.Name.c_str(), Val);
		break;
	}
	case EPropertyType::ByteBool:
	{
		uint8* Val = static_cast<uint8*>(Prop.ValuePtr);
		bool bVal = (*Val != 0);
		if (ImGui::Checkbox(Prop.Name.c_str(), &bVal))
		{
			*Val = bVal ? 1 : 0;
			bChanged = true;
		}
		break;
	}
	case EPropertyType::Int:
	{
		int32* Val = static_cast<int32*>(Prop.ValuePtr);
		if (Prop.Min != 0.0f || Prop.Max != 0.0f)
			bChanged = ImGui::DragInt(Prop.Name.c_str(), Val, (int32)Prop.Speed, (int32)Prop.Min, (int32)Prop.Max);
		else
			bChanged = ImGui::DragInt(Prop.Name.c_str(), Val, (int32)Prop.Speed);
		break;
	}
	case EPropertyType::Float:
	{
		float* Val = static_cast<float*>(Prop.ValuePtr);
		if (Prop.Min != 0.0f || Prop.Max != 0.0f)
			bChanged = ImGui::DragFloat(Prop.Name.c_str(), Val, Prop.Speed, Prop.Min, Prop.Max);
		else
			bChanged = ImGui::DragFloat(Prop.Name.c_str(), Val, Prop.Speed);
		break;
	}
	case EPropertyType::Vec3:
	{
		float* Val = static_cast<float*>(Prop.ValuePtr);
		bChanged = ImGui::DragFloat3(Prop.Name.c_str(), Val, Prop.Speed);
		break;
	}
	case EPropertyType::Rotator:
	{
		// FRotator 메모리 레이아웃 [Pitch,Yaw,Roll] → UI X=Roll(X축), Y=Pitch(Y축), Z=Yaw(Z축)
		FRotator* Rot = static_cast<FRotator*>(Prop.ValuePtr);
		float RotXYZ[3] = { Rot->Roll, Rot->Pitch, Rot->Yaw };
		bChanged = ImGui::DragFloat3(Prop.Name.c_str(), RotXYZ, Prop.Speed);
		if (bChanged)
		{
			Rot->Roll = RotXYZ[0];
			Rot->Pitch = RotXYZ[1];
			Rot->Yaw = RotXYZ[2];
			if (SelectedComponent && SelectedComponent->IsA<USceneComponent>())
			{
				static_cast<USceneComponent*>(SelectedComponent)->ApplyCachedEditRotator();
			}
		}
		break;
	}
	case EPropertyType::Vec4:
	{
		float* Val = static_cast<float*>(Prop.ValuePtr);
		bChanged = ImGui::DragFloat4(Prop.Name.c_str(), Val, Prop.Speed);
		break;
	}
	case EPropertyType::Color4:
	{
		float* Val = static_cast<float*>(Prop.ValuePtr);
		bChanged = ImGui::ColorEdit4(Prop.Name.c_str(), Val);
		break;
	}
	case EPropertyType::String:
	{
		FString* Val = static_cast<FString*>(Prop.ValuePtr);
		char Buf[256];
		strncpy_s(Buf, sizeof(Buf), Val->c_str(), _TRUNCATE);
		if (ImGui::InputText(Prop.Name.c_str(), Buf, sizeof(Buf)))
		{
			*Val = Buf;
			bChanged = true;
		}
		break;
	}
	case EPropertyType::SceneComponentRef:
	{
		FString* Val = static_cast<FString*>(Prop.ValuePtr);
		UMovementComponent* MovementComp = SelectedComponent ? Cast<UMovementComponent>(SelectedComponent) : nullptr;
		FString Preview = MovementComp ? MovementComp->GetUpdatedComponentDisplayName() : FString("None");

		if (ImGui::BeginCombo(Prop.Name.c_str(), Preview.c_str()))
		{
			bool bSelectedAuto = Val->empty();
			if (ImGui::Selectable("Auto (Root)", bSelectedAuto))
			{
				Val->clear();
				bChanged = true;
			}
			if (bSelectedAuto)
			{
				ImGui::SetItemDefaultFocus();
			}

			if (MovementComp)
			{
				for (USceneComponent* Candidate : MovementComp->GetOwnerSceneComponents())
				{
					if (!Candidate)
					{
						continue;
					}

					FString CandidatePath = MovementComp->BuildUpdatedComponentPath(Candidate);
					FString CandidateName = Candidate->GetFName().ToString();
					if (CandidateName.empty())
					{
						CandidateName = Candidate->GetClass()->GetName();
					}
					if (!CandidatePath.empty())
					{
						CandidateName += " (" + CandidatePath + ")";
					}

					bool bSelected = (*Val == CandidatePath);
					if (ImGui::Selectable(CandidateName.c_str(), bSelected))
					{
						*Val = CandidatePath;
						bChanged = true;
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
			}

			ImGui::EndCombo();
		}
		break;
	}
	case EPropertyType::StaticMeshRef:
	{
		FString* Val = static_cast<FString*>(Prop.ValuePtr);

		FString Preview = Val->empty() ? "None" : GetStemFromPath(*Val);
		if (*Val == "None") Preview = "None";

		ImGui::Text("%s", Prop.Name.c_str());
		ImGui::SameLine(120);

		float ButtonWidth = ImGui::CalcTextSize("Import OBJ").x + ImGui::GetStyle().FramePadding.x * 2.0f;
		float Spacing = ImGui::GetStyle().ItemSpacing.x;
		ImGui::SetNextItemWidth(-(ButtonWidth + Spacing));

		if (ImGui::BeginCombo("##Mesh", Preview.c_str()))
		{
			bool bSelectedNone = (*Val == "None");
			if (ImGui::Selectable("None", bSelectedNone))
			{
				*Val = "None";
				bChanged = true;
			}
			if (bSelectedNone)
				ImGui::SetItemDefaultFocus();

			const TArray<FMeshAssetListItem>& MeshFiles = FObjManager::GetAvailableMeshFiles();
			for (const FMeshAssetListItem& Item : MeshFiles)
			{
				bool bSelected = (*Val == Item.FullPath);
				if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
				{
					*Val = Item.FullPath;
					bChanged = true;
				}
				if (bSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		// .obj 임포트 버튼
		ImGui::SameLine();

		ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ButtonWidth);
		if (ImGui::Button("Import OBJ"))
		{
			FString ObjPath = OpenObjFileDialog();
			if (!ObjPath.empty())
			{
				ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
				UStaticMesh* Loaded = FObjManager::LoadObjStaticMesh(ObjPath, Device);
				if (Loaded)
				{
					*Val = FObjManager::GetBinaryFilePath(ObjPath);
					bChanged = true;
				}
			}
		}
		break;
	}
	case EPropertyType::MaterialSlot:
	{
		FMaterialSlot* Slot = static_cast<FMaterialSlot*>(Prop.ValuePtr);
		int32          ElemIdx = (strncmp(Prop.Name.c_str(), "Element ", 8) == 0) ? atoi(&Prop.Name[8]) : -1;

		FString SlotName = "None";
		if (ElemIdx != -1 && SelectedComponent && SelectedComponent->IsA<UStaticMeshComponent>())
		{
			UStaticMeshComponent* SMC = static_cast<UStaticMeshComponent*>(SelectedComponent);
			if (SMC->GetStaticMesh() && ElemIdx < (int32)SMC->GetStaticMesh()->GetStaticMaterials().size())
				SlotName = SMC->GetStaticMesh()->GetStaticMaterials()[ElemIdx].MaterialSlotName;
		}

		// 좌측: Element 인덱스 + 슬롯 이름
		ImGui::BeginGroup();
		ImGui::Text("Element %d", ElemIdx);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
		ImGui::TextUnformatted(SlotName.c_str());
		ImGui::PopStyleColor();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", SlotName.c_str());
		ImGui::EndGroup();

		ImGui::SameLine(120);

		// 우측: Material 콤보
		ImGui::BeginGroup();
		ImGui::SetNextItemWidth(-1);

		FString Preview = (Slot->Path.empty() || Slot->Path == "None") ? "None" : Slot->Path;
		if (ImGui::BeginCombo("##Mat", Preview.c_str()))
		{
			// "None" 선택지 기본 제공
			bool bSelectedNone = (Slot->Path == "None" || Slot->Path.empty());
			if (ImGui::Selectable("None", bSelectedNone))
			{
				Slot->Path = "None";
				bChanged = true;
			}
			if (bSelectedNone) ImGui::SetItemDefaultFocus();

			// TObjectIterator 대신 FMaterialManager 파일 목록 스캔 데이터 사용
			const TArray<FMaterialAssetListItem>& MatFiles = FMaterialManager::Get().GetAvailableMaterialFiles();
			for (const FMaterialAssetListItem& Item : MatFiles)
			{
				bool bSelected = (Slot->Path == Item.FullPath);
				if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
				{
					Slot->Path = Item.FullPath; // 데이터는 전체 경로로 저장
					bChanged = true;
				}
				if (bSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MaterialContentItem"))
			{
				FContentItem ContentItem = *reinterpret_cast<const FContentItem*>(payload->Data);
				Slot->Path = FPaths::ToUtf8(
					ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring()
				);
				bChanged = true;
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::EndGroup();
		break;
	}
	case EPropertyType::Name:
	{
		FName* Val = static_cast<FName*>(Prop.ValuePtr);
		FString Current = Val->ToString();

		// 리소스 키와 매칭되는 프로퍼티면 콤보 박스로 렌더링
		TArray<FString> Names;
		if (strcmp(Prop.Name.c_str(), "Font") == 0)
			Names = FResourceManager::Get().GetFontNames();
		else if (strcmp(Prop.Name.c_str(), "Particle") == 0)
			Names = FResourceManager::Get().GetParticleNames();
		else if (strcmp(Prop.Name.c_str(), "Texture") == 0)
			Names = FResourceManager::Get().GetTextureNames();

		if (!Names.empty())
		{
			if (ImGui::BeginCombo(Prop.Name.c_str(), Current.c_str()))
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
			if (ImGui::InputText(Prop.Name.c_str(), Buf, sizeof(Buf)))
			{
				*Val = FName(Buf);
				bChanged = true;
			}
		}
		break;
	}
	}

	if (bChanged && SelectedComponent)
	{
		SelectedComponent->PostEditProperty(Prop.Name.c_str());
	}

	ImGui::PopID();
	return bChanged;
}
