#include "Editor/UI/Panel/EditorPropertyWidget.h"
#include "Editor/EditorEngine.h"

#include "ImGui/imgui.h"
#include "Component/ActorComponent.h"
#include "Component/Primitive/BillboardComponent.h"
#include "Component/MeshComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "Component/Debug/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/Primitive/TextRenderComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "Component/Primitive/DecalComponent.h"
#include "Component/Primitive/HeightFogComponent.h"
#include "GameFramework/AActor.h"
#include "Asset/AssetRegistry.h"
#include "Core/Property/ClassProperty.h"
#include "Core/Property/ArrayProperty.h"
#include "Core/Property/NumericProperty.h"
#include "Core/Property/ObjectProperty.h"
#include "Core/Property/StructProperty.h"
#include "Core/Property/SoftObjectProperty.h"
#include "Core/Types/ClassTypes.h"
#include "Math/FloatCurve.h"
#include "Lua/LuaScriptManager.h"
#include "Resource/ResourceManager.h"
#include "Object/FName.h"
#include "Object/ObjectIterator.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Materials/Material.h"
#include "Mesh/Importer/MeshImportOptions.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Editor/UI/Asset/Mesh/MeshEditorWidget.h"
#include "Platform/Paths.h"
#include "Serialization/MemoryArchive.h"

#include <Windows.h>
#include <commdlg.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cfloat>
#include <cstring>
#include <filesystem>
#include <cstdio>
#include <utility>

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

	const char* GetPropertyDisplayName(const FPropertyValue& Prop)
	{
		return Prop.GetDisplayName();
	}

	void DispatchPostEditChange(
		const FPropertyValue& Prop,
		EPropertyChangeType ChangeType = EPropertyChangeType::ValueSet,
		int32 ArrayIndex = -1,
		const FString& PropertyPath = {},
		const char* OverridePropertyName = nullptr,
		const char* OverrideDisplayName = nullptr)
	{
		if (!Prop.Object)
		{
			return;
		}

		FPropertyChangedEvent Event;
		Event.Object = Prop.Object;
		Event.Property = Prop.Property;
		Event.PropertyName = OverridePropertyName ? OverridePropertyName : Prop.GetName();
		Event.DisplayName = OverrideDisplayName ? OverrideDisplayName : GetPropertyDisplayName(Prop);
		Event.PropertyPath = PropertyPath.empty() ? Prop.GetName() : PropertyPath;
		Event.Type = Prop.GetType();
		Event.ChangeType = ChangeType;
		Event.ArrayIndex = ArrayIndex;
		Prop.Object->PostEditChangeProperty(Event);
	}

	bool CopyPropertyValue(const FPropertyValue& SrcValue, FPropertyValue& DstValue)
	{
		void* SrcPtr = SrcValue.GetValuePtr();
		void* DstPtr = DstValue.GetValuePtr();
		if (!SrcPtr || !DstPtr)
		{
			return false;
		}

		const FSoftObjectProperty* SrcSoftProperty = SrcValue.Property ? SrcValue.Property->AsSoftObjectProperty() : nullptr;
		const FSoftObjectProperty* DstSoftProperty = DstValue.Property ? DstValue.Property->AsSoftObjectProperty() : nullptr;
		if (SrcSoftProperty || DstSoftProperty)
		{
			if (!SrcSoftProperty || !DstSoftProperty)
			{
				return false;
			}

			DstSoftProperty->SetPath(DstValue.ContainerPtr, SrcSoftProperty->GetPath(SrcValue.ContainerPtr));
			return true;
		}

		if (SrcValue.GetType() != DstValue.GetType())
		{
			return false;
		}

		size_t Size = 0;
		switch (SrcValue.GetType())
		{
		case EPropertyType::Bool:          Size = sizeof(bool); break;
		case EPropertyType::ByteBool:      Size = sizeof(uint8); break;
		case EPropertyType::Int:           Size = sizeof(int32); break;
		case EPropertyType::Float:         Size = sizeof(float); break;
		case EPropertyType::Vec3:
		case EPropertyType::Rotator:       Size = sizeof(float) * 3; break;
		case EPropertyType::Vec4:
		case EPropertyType::Color4:        Size = sizeof(float) * 4; break;
		case EPropertyType::Enum:          Size = SrcValue.GetEnumType() ? SrcValue.GetEnumType()->GetSize() : sizeof(int32); break;
		case EPropertyType::String:
			*static_cast<FString*>(DstPtr) = *static_cast<FString*>(SrcPtr);
			return true;
		case EPropertyType::ObjectRef:
			*static_cast<UObject**>(DstPtr) = *static_cast<UObject**>(SrcPtr);
			return true;
		case EPropertyType::ClassRef:
		{
			const FClassProperty* SrcClassProperty = SrcValue.Property ? SrcValue.Property->AsClassProperty() : nullptr;
			const FClassProperty* DstClassProperty = DstValue.Property ? DstValue.Property->AsClassProperty() : nullptr;
			if (!SrcClassProperty || !DstClassProperty)
			{
				return false;
			}
			DstClassProperty->SetClassValue(DstValue.ContainerPtr, SrcClassProperty->GetClassValue(SrcValue.ContainerPtr));
			return true;
		}
		case EPropertyType::Name:
			*static_cast<FName*>(DstPtr) = *static_cast<FName*>(SrcPtr);
			return true;
		case EPropertyType::Array:
		{
			FPropertySerializeContext SrcContext;
			SrcContext.Owner = SrcValue.Object;
			FMemoryArchive Writer(/*bInIsSaving=*/true);
			SrcValue.Property->SerializeValue(SrcPtr, Writer, SrcContext);

			FPropertySerializeContext DstContext;
			DstContext.Owner = DstValue.Object;
			FMemoryArchive Reader(Writer.GetBuffer(), /*bInIsSaving=*/false);
			DstValue.Property->SerializeValue(DstPtr, Reader, DstContext);
			return true;
		}
		case EPropertyType::Struct:
		{
			if (!SrcValue.GetStructType() || !DstValue.GetStructType())
			{
				return false;
			}

			TArray<FPropertyValue> SrcChildren;
			TArray<FPropertyValue> DstChildren;
			SrcValue.GetStructChildren(SrcChildren);
			DstValue.GetStructChildren(DstChildren);

			bool bCopiedAny = false;
			for (const FPropertyValue& SrcChild : SrcChildren)
			{
				for (FPropertyValue& DstChild : DstChildren)
				{
					if (std::strcmp(SrcChild.GetName(), DstChild.GetName()) == 0 && CopyPropertyValue(SrcChild, DstChild))
					{
						bCopiedAny = true;
						break;
					}
				}
			}
			return bCopiedAny;
		}
		default:
			return false;
		}

		if (Size > 0)
		{
			memcpy(DstPtr, SrcPtr, Size);
			return true;
		}

		return false;
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
		bShowDuplicateWarning = false;
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
		ImGui::SetWindowFontScale(1.5f);
		ImGui::Text(PrimaryActor->GetFName().ToString().c_str());
		ImGui::SetWindowFontScale(1.0f);

		if (ImGui::IsItemClicked())
		{
			bActorSelected = true;
			SelectedComponent = nullptr;
		}
		//ImGui::SameLine();

		//ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70.0f);
		//ImGui::InputText("##Rename", RenameBuffer, sizeof(RenameBuffer));
		//ImGui::SameLine();
		//if (ImGui::Button("Rename"))
		//{
		//	RenameActor(PrimaryActor);
		//}
	}

	if (bShowDuplicateWarning)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
		ImGui::Text("이미 사용 중인 이름입니다.");
		ImGui::PopStyleColor();
	}

	// ========== 고정 영역: Component Tree ==========
	RenderComponentTree(PrimaryActor);

	// ========== 스크롤 영역: Details ==========
	float ScrollHeight = ImGui::GetContentRegionAvail().y;
	if (ScrollHeight < 50.0f) ScrollHeight = 50.0f;

	ImGui::BeginChild("##Details", ImVec2(0, ScrollHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	{
		RenderDetails(PrimaryActor, SelectedActors);
	}
	ImGui::EndChild();

	ImGui::End();
}

void FEditorPropertyWidget::RenameActor(AActor* PrimaryActor)
{
	FString NewName(RenameBuffer);
	FString CurrentName = PrimaryActor->GetFName().ToString();

	// 현재 이름과 동일하면 스킵
	if (NewName == CurrentName)
	{
		RenameBuffer[0] = '\0';
		return;
	}
		
	// 월드의 모든 Actor를 순회하며 중복 이름 체크
	bShowDuplicateWarning = false;
	UWorld* World = EditorEngine->GetWorld();
	if (World)
	{
		for (AActor* Actor : World->GetActors()) 
		{
			if (Actor == PrimaryActor) continue;
			if (Actor->GetFName().ToString() == NewName)
			{
				bShowDuplicateWarning = true;
				break;
			}
		}
	}

	if (!bShowDuplicateWarning)
	{
		PrimaryActor->SetFName(FName(NewName));
		strncpy_s(RenameBuffer, sizeof(RenameBuffer),
			NewName.c_str(), _TRUNCATE);
	}

	RenameBuffer[0] = '\0';
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
	if (PrimaryActor->GetRootComponent())
	{
		ImGui::Separator();
		ImGui::Text("Transform");
		ImGui::Spacing();

		TArray<FPropertyValue> Props;
		PrimaryActor->GetEditableProperties(Props);

		if (ImGui::BeginTable("##ActorPropertyTable", 2,
			ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 275.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.145f, 0.145f, 0.145f, 1.0f));

			for (int32 i = 0; i < (int32)Props.size(); ++i)
			{
				ImGui::TableNextRow();
				ImGui::PushID(i);

				ImGui::TableSetColumnIndex(0);
				const bool bPropertyOpen = FEditorPropertyRenderer::DrawPropertyLabel(Props[i]);

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1);

				FEditorPropertyRenderOptions Options;
				Options.bUseExternalExpansion = true;
				Options.bParentExpanded = bPropertyOpen;
				Options.EditedSceneComponent = Cast<USceneComponent>(SelectedComponent);
				PropertyRenderer.RenderPropertyWidget(Props, i, Options);
				ImGui::PopID();
			}

			ImGui::EndTable();
			ImGui::PopStyleColor(2);
		}
	}
}

void FEditorPropertyWidget::RenderComponentTree(AActor* Actor)
{
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

	ImGui::SameLine();

	if (ImGui::Button("Add"))
	{
		ImGui::OpenPopup("##AddComponentPopup");
	}

	if (ImGui::BeginPopup("##AddComponentPopup"))
	{
		auto AddComponentClassItem = [&](UClass* Cls)
		{
			if (ImGui::Selectable(Cls->GetName()))
			{
				AddComponentToActor(Actor, Cls);
				ImGui::CloseCurrentPopup();
			}
		};

		for (const FComponentClassGroup& Group : ComponentGroups)
		{
			if (Group.Classes.empty()) continue;

			if (ImGui::TreeNode(Group.Label))
			{
				for (UClass* Cls : Group.Classes)
				{
					AddComponentClassItem(Cls);
				}

				ImGui::TreePop();
			}
		}

		if (!OtherClasses.empty())
		{
			if (ImGui::TreeNode("Other"))
			{
				for (UClass* Cls : OtherClasses)
				{
					AddComponentClassItem(Cls);
				}

				ImGui::TreePop();
			}
		}

		ImGui::EndPopup();
	}

	ImGui::Separator();

	USceneComponent* Root = Actor->GetRootComponent();

	static float TreeHeight = 100.0f;

	ImGui::BeginChild("##ComponentTree", ImVec2(0, TreeHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	{
		if (Root)
		{
			RenderSceneComponentNode(Root);
		}

		TArray<UActorComponent*> NonSceneComponents;
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (!Comp) continue;
			if (Comp->IsA<USceneComponent>()) continue;
			if (ShouldHideInComponentTree(Comp, bShowEditorOnlyComponents)) continue;
			NonSceneComponents.push_back(Comp);
		}

		if (!NonSceneComponents.empty())
		{
			ImGui::Separator();
		}

		for (UActorComponent* Comp : NonSceneComponents)
		{
			FString Name = Comp->GetFName().ToString();
			const FString TypeName = Comp->GetClass()->GetName();
			const FString DefaultNamePrefix = TypeName + "_";

			const bool bUseTypeAsLabel = Name.empty() || Name == TypeName || Name.rfind(DefaultNamePrefix, 0) == 0;

			const char* Label = bUseTypeAsLabel ? TypeName.c_str() : Name.c_str();

			ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

			if (!bActorSelected && SelectedComponent == Comp)
			{
				Flags |= ImGuiTreeNodeFlags_Selected;
			}

			ImGui::TreeNodeEx(Comp, Flags, "%s", Label);
		
			if (ImGui::IsItemClicked())
			{
				SelectedComponent = Comp;
				bActorSelected = false;
			}
		}
	}

	ImGui::EndChild();

	ImGui::InvisibleButton("##TreeResize", ImVec2(-1, 6));

	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	}

	if (ImGui::IsItemActive())
	{
		TreeHeight += ImGui::GetIO().MouseDelta.y;
		TreeHeight = std::max(TreeHeight, 80.0f);
	}

	ImVec2 Min = ImGui::GetItemRectMin();
	ImVec2 Max = ImGui::GetItemRectMax();

	ImU32 Color =
		ImGui::GetColorU32(
			ImGui::IsItemHovered()
			? ImGuiCol_SeparatorHovered
			: ImGuiCol_Separator
		);

	ImGui::GetWindowDrawList()->AddLine(
		ImVec2(Min.x, (Min.y + Max.y) * 0.5f),
		ImVec2(Max.x, (Min.y + Max.y) * 0.5f),
		Color,
		2.0f
	);
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

	// reflected property 기반 자동 위젯 렌더링
	TArray<FPropertyValue> Props;
	SelectedComponent->GetEditableProperties(Props);

	bool bIsRoot = false;
	if (SelectedComponent->IsA<USceneComponent>())
	{
		USceneComponent* SceneComp = static_cast<USceneComponent*>(SelectedComponent);
		bIsRoot = (SceneComp->GetParent() == nullptr);
	}

	// 카테고리 순서 수집 (등장 순 유지)
	TArray<std::string> CategoryOrder;
	for (const auto& P : Props)
	{
		const char* PropertyCategory = P.GetCategory();
		bool bFound = false;
		for (const auto& C : CategoryOrder)
		{
			if (C == PropertyCategory) { bFound = true; break; }
		}
		if (!bFound) CategoryOrder.push_back(PropertyCategory);
	}

	bool bAnyChanged = false;
	// Static mesh path 변경은 SetStaticMesh를 통해 MaterialSlots를 resize 하므로
	// Props에 들어있던 &MaterialSlots[i] 포인터가 모두 무효화된다. 이후 Materials
	// 카테고리 등을 더 렌더링하면 dangling pointer 접근 → bad_alloc.
	// 변경이 발생하면 즉시 외부 루프까지 빠져나와 다음 프레임에 Props를 새로 수집해 렌더한다.
	bool bPropsInvalidated = false;

	for (const auto& Cat : CategoryOrder)
	{
		if (bPropsInvalidated) break;

		// Root 컴포넌트는 Transform 카테고리 스킵
		if (bIsRoot && Cat == "Transform")
			continue;

		// 카테고리 헤더 (빈 문자열이면 헤더 없이 렌더)
		bool bInTreeNode = false;
		if (!Cat.empty())
		{
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.27f, 0.27f, 0.27f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.30f, 0.30f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 3.0f));

			bool bOpen = ImGui::CollapsingHeader(Cat.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

			ImGui::PopStyleVar();
			ImGui::PopStyleColor(3);

			if (!bOpen) continue;
		}

		if (ImGui::BeginTable("##PropertyTable", 2,
			ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 275.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.145f, 0.145f, 0.145f, 1.0f));

			for (int32 i = 0; i < (int32)Props.size(); ++i)
			{
				if (Cat != Props[i].GetCategory())
					continue;

				ImGui::TableNextRow();
				ImGui::PushID(i);

				ImGui::TableSetColumnIndex(0);
				const bool bPropertyOpen = FEditorPropertyRenderer::DrawPropertyLabel(Props[i]);

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1);

				FEditorPropertyRenderOptions Options;
				Options.bUseExternalExpansion = true;
				Options.bParentExpanded = bPropertyOpen;
				Options.EditedSceneComponent = Cast<USceneComponent>(SelectedComponent);
				bool bChanged = PropertyRenderer.RenderPropertyWidget(Props, i, Options);

				if (bChanged)
				{
					bAnyChanged = true;
					PropagatePropertyChange(Props[i].GetName(), SelectedActors);

					// 모든 변경 후 props 재수집 — 같은 frame 의 후속 prop 들이 dangling pointer 를
					// 참조하는 케이스 방지. 예: SkeletalMeshComponent 의 AnimationMode/AnimInstanceClass
					// 변경 시 InitializeAnimation 이 AnimInstance 를 swap 하므로, forward 됐던
					// AnimInstance 의 prop 들의 ContainerPtr 가 destroyed 인스턴스를 가리키게 된다.
					// 사용자는 frame 당 1 prop 변경이 일반적이라 UX 영향 거의 없음.
					bPropsInvalidated = true;
					ImGui::PopID();
					break;
				}
				ImGui::PopID();
			}

			ImGui::EndTable();
			ImGui::PopStyleColor(2);
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
	TArray<FPropertyValue> SrcProps;
	SelectedComponent->GetEditableProperties(SrcProps);

	const FPropertyValue* SrcProp = nullptr;
	for (const auto& P : SrcProps)
	{
		if (P.GetName() == PropName) { SrcProp = &P; break; }
	}
	if (!SrcProp) return;
	FPropertyValue SrcValue = *SrcProp;

	for (AActor* Actor : SelectedActors)
	{
		if (!Actor || Actor == PrimaryActor) continue;

		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (!Comp || Comp->GetClass() != CompClass) continue;

			TArray<FPropertyValue> DstProps;
			Comp->GetEditableProperties(DstProps);

			for (FPropertyValue& DstProp : DstProps)
			{
				if (!DstProp.Property || DstProp.GetName() != PropName || DstProp.GetType() != SrcProp->GetType()) continue;
				if (!DstProp.GetValuePtr() || !SrcValue.GetValuePtr()) continue;

				if (CopyPropertyValue(SrcValue, DstProp))
				{
					DispatchPostEditChange(DstProp);
				}
				break;
			}
			break; // 같은 타입의 첫 번째 컴포넌트에만 전파
		}
	}
}

void FEditorPropertyWidget::AddComponentToActor(AActor* Actor, UClass* ComponentClass)
{
	if (!Actor || !ComponentClass) return;

	UActorComponent* Comp = Actor->AddComponentByClass(ComponentClass);
	if (!Comp) return;

	if (ComponentClass->IsA(USceneComponent::StaticClass()))
	{
		USceneComponent* Root = Actor->GetRootComponent();
		USceneComponent* SceneComp = Cast<USceneComponent>(Comp);

		if (SelectedComponent && SelectedComponent->IsA<USceneComponent>())
		{
			SceneComp->AttachToComponent(Cast<USceneComponent>(SelectedComponent));
		}
		else
		{
			SceneComp->AttachToComponent(Root);
		}

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

	SelectedComponent = Comp;
	bActorSelected = false;
}
