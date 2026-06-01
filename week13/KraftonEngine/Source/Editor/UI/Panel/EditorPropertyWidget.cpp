#include "Editor/UI/Panel/EditorPropertyWidget.h"
#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"

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
#include "Object/Object.h"
#include "Object/ObjectIterator.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Object/Reflection/UStruct.h"
#include "Materials/Material.h"
#include "Mesh/Importer/MeshImportOptions.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsAssetManager.h"
#include "Editor/UI/Asset/Mesh/MeshEditorWidget.h"
#include "Platform/Paths.h"
#include "Serialization/MemoryArchive.h"
#include "Core/Logging/Log.h"

#include <Windows.h>
#include <commdlg.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cfloat>
#include <cstring>
#include <filesystem>
#include <cstdio>
#include <utility>

#include "Materials/MaterialManager.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

namespace
{
	bool IsFbxFilePath(const FString& Path)
	{
		std::filesystem::path FilePath(FPaths::ToWide(Path));
		std::wstring Extension = FilePath.extension().wstring();
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
		return Extension == L".fbx";
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

	const FString* FindPropertyMetadata(const FPropertyValue& Prop, const FString& Key)
	{
		const TMap<FString, FString>& Metadata = Prop.GetMetadata();
		auto It = Metadata.find(Key);
		return It != Metadata.end() ? &It->second : nullptr;
	}

	bool IsTruthyMetadataValue(const FString& Value)
	{
		return Value.empty() || Value == "true" || Value == "1" || Value == "yes";
	}

	bool HasTruthyPropertyMetadata(const FPropertyValue& Prop, const FString& Key)
	{
		if (const FString* Value = FindPropertyMetadata(Prop, Key))
		{
			return IsTruthyMetadataValue(*Value);
		}
		return false;
	}

	FString GetAssetTypeMetadata(const FPropertyValue& Prop)
	{
		if (const FString* AssetType = FindPropertyMetadata(Prop, "assettype"))
		{
			return *AssetType;
		}
		if (const FString* AllowedClass = FindPropertyMetadata(Prop, "allowedclass"))
		{
			return *AllowedClass;
		}
		return {};
	}

	UClass* GetAllowedClassMetadata(const FPropertyValue& Prop)
	{
		if (const FString* AllowedClass = FindPropertyMetadata(Prop, "allowedclass"))
		{
			return UClass::FindByName(AllowedClass->c_str());
		}
		return nullptr;
	}

	FString MakePropertyPath(const FString& ParentPath, const char* PropertyName)
	{
		if (!PropertyName || PropertyName[0] == '\0')
		{
			return ParentPath;
		}
		if (ParentPath.empty())
		{
			return PropertyName;
		}
		return ParentPath + "." + PropertyName;
	}

	FString MakeArrayElementPath(const FString& ArrayPath, int32 ArrayIndex)
	{
		return ArrayPath + "[" + std::to_string(ArrayIndex) + "]";
	}

	AActor* GetPropertyOwnerActor(const FPropertyValue& Prop)
	{
		if (AActor* Actor = Cast<AActor>(Prop.Object))
		{
			return Actor;
		}
		if (UActorComponent* Component = Cast<UActorComponent>(Prop.Object))
		{
			return Component->GetOwner();
		}
		return nullptr;
	}

	TArray<UObject*> GetOwnerObjectReferenceChoices(const FPropertyValue& Prop, UClass* AllowedClass)
	{
		TArray<UObject*> Choices;
		if (!AllowedClass)
		{
			return Choices;
		}

		AActor* OwnerActor = GetPropertyOwnerActor(Prop);
		if (!OwnerActor)
		{
			return Choices;
		}

		if (OwnerActor->GetClass()->IsA(AllowedClass))
		{
			Choices.push_back(OwnerActor);
		}

		for (UActorComponent* Component : OwnerActor->GetComponents())
		{
			if (Component && Component->GetClass()->IsA(AllowedClass))
			{
				Choices.push_back(Component);
			}
		}

		return Choices;
	}

	FString GetObjectReferenceChoiceLabel(const UObject* Object)
	{
		if (!Object)
		{
			return "None";
		}

		FString Label = Object->GetFName().ToString();
		if (Label.empty())
		{
			Label = Object->GetClass()->GetName();
		}
		return Label;
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
		{
			const FObjectProperty* SrcObjectProperty = SrcValue.Property ? SrcValue.Property->AsObjectProperty()
			: nullptr;
			const FObjectProperty* DstObjectProperty = DstValue.Property ? DstValue.Property->AsObjectProperty()
			: nullptr;
			if (!SrcObjectProperty || !DstObjectProperty)
			{
				return false;
			}

			DstObjectProperty->SetObjectValueFromValuePtr(
				DstPtr,
				SrcObjectProperty->GetObjectValueFromValuePtr(SrcPtr)
			);
			return true;
		}
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

	bool RenderClassPropertyWidget(FPropertyValue& Prop)
	{
		const FClassProperty* ClassProperty = Prop.Property ? Prop.Property->AsClassProperty() : nullptr;
		if (!ClassProperty || !Prop.GetValuePtr())
		{
			return false;
		}

		UClass* AllowedClass = ClassProperty->GetAllowedClassType();
		if (!AllowedClass)
		{
			AllowedClass = GetAllowedClassMetadata(Prop);
		}
		UClass* CurrentClass = ClassProperty->GetClassValue(Prop.ContainerPtr);
		FString Preview = CurrentClass ? CurrentClass->GetName() : FString("None");
		bool bChanged = false;

		if (ImGui::BeginCombo("##Value", Preview.c_str()))
		{
			const bool bSelectedNone = CurrentClass == nullptr;
			if (ImGui::Selectable("None", bSelectedNone))
			{
				ClassProperty->SetClassValue(Prop.ContainerPtr, nullptr);
				bChanged = true;
			}
			if (bSelectedNone)
			{
				ImGui::SetItemDefaultFocus();
			}

			TArray<UClass*>& Classes = UClass::GetAllClasses();
			for (UClass* Candidate : Classes)
			{
				if (!Candidate)
				{
					continue;
				}
				if (AllowedClass && !Candidate->IsA(AllowedClass))
				{
					continue;
				}

				const bool bSelected = Candidate == CurrentClass;
				if (ImGui::Selectable(Candidate->GetName(), bSelected))
				{
					ClassProperty->SetClassValue(Prop.ContainerPtr, Candidate);
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

FString FEditorPropertyWidget::OpenStaticMeshFileDialog()
{
	wchar_t FilePath[MAX_PATH] = {};

	OPENFILENAMEW Ofn = {};
	Ofn.lStructSize = sizeof(Ofn);
	Ofn.hwndOwner = nullptr;
	Ofn.lpstrFilter = L"Static Mesh Files (*.obj;*.fbx)\0*.obj;*.fbx\0OBJ Files (*.obj)\0*.obj\0FBX Files (*.fbx)\0*.fbx\0All Files (*.*)\0*.*\0";
	Ofn.lpstrFile = FilePath;
	Ofn.nMaxFile = MAX_PATH;
	Ofn.lpstrTitle = L"Import Static Mesh";
	Ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (GetOpenFileNameW(&Ofn))
	{
		std::filesystem::path AbsPath = std::filesystem::path(FilePath).lexically_normal();
		std::filesystem::path RootPath = std::filesystem::path(FPaths::RootDir());
		std::filesystem::path RelPath = AbsPath.lexically_relative(RootPath);

		if (RelPath.empty() || RelPath.wstring().starts_with(L".."))
		{
			return FPaths::ToUtf8(AbsPath.generic_wstring());
		}
		return FPaths::ToUtf8(RelPath.generic_wstring());
	}

	return FString();
}

FString FEditorPropertyWidget::OpenFbxFileDialog()
{
	wchar_t FilePath[MAX_PATH] = {};
	OPENFILENAMEW Ofn = {};
	Ofn.lStructSize = sizeof(Ofn);
	Ofn.hwndOwner = nullptr;
	Ofn.lpstrFilter = L"FBX Files (*.fbx)\0*.fbx\0All Files (*.*)\0*.*\0";
	Ofn.lpstrFile = FilePath;
	Ofn.nMaxFile = MAX_PATH;
	Ofn.lpstrTitle = L"Import FBX Mesh";
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

void FEditorPropertyWidget::Render(const FEditorPanelContext& Context)
{
	SelectionManager = Context.SelectionManager;
	if (!SelectionManager)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(350.0f, 500.0f), ImGuiCond_Once);
	ImGui::Begin("Property Window");

	const FSelectionDetailTarget* PrimaryTarget = SelectionManager->GetPrimaryDetailTarget();
	if (!PrimaryTarget || !PrimaryTarget->IsValidTarget())
	{
		LastSelectedActor = nullptr;
		ImGui::Text("No object selected.");
		ImGui::End();
		return;
	}

	AActor* PrimaryActor = SelectionManager->GetPrimarySelection();
	if (PrimaryActor != LastSelectedActor)
	{
		LastSelectedActor = PrimaryActor;
		bShowDuplicateWarning = false;
	}

	const TArray<FSelectionDetailTarget>& SelectedTargets = SelectionManager->GetSelectedDetailTargets();
	const int32 SelectionCount = static_cast<int32>(SelectedTargets.size());
	UObject* PrimaryObject = PrimaryTarget->ObjectPtr;

	if (AActor* Actor = Cast<AActor>(PrimaryObject))
	{
		if (SelectionCount > 1)
		{
			ImGui::Text("Class: %s", Actor->GetClass()->GetName());
			FString PrimaryName = Actor->GetFName().ToString();
			if (PrimaryName.empty()) PrimaryName = Actor->GetClass()->GetName();
			ImGui::Text("Name: %s (+%d)", PrimaryName.c_str(), SelectionCount - 1);

			ImGui::SameLine();
			char RemoveLabel[64];
			snprintf(RemoveLabel, sizeof(RemoveLabel), "Remove %d Objects", SelectionCount);
			if (ImGui::Button(RemoveLabel))
			{
				TArray<AActor*> ToDelete = SelectionManager->GetSelectedActors();
				SelectionManager->ClearSelection();
				for (AActor* ActorToDelete : ToDelete)
				{
					if (IsValid(ActorToDelete))
					{
						UWorld* ActorWorld = ActorToDelete->GetWorld();
						if (ActorWorld)
						{
							ActorWorld->DestroyActor(ActorToDelete);
						}
					}
				}
				EditorEngine->InvalidateOcclusionResults();
				LastSelectedActor = nullptr;
				ImGui::End();
				return;
			}
		}
		else
		{
			ImGui::SetWindowFontScale(1.5f);
			ImGui::Text("%s", Actor->GetFName().ToString().c_str());
			ImGui::SetWindowFontScale(1.0f);
		}
	}
	else if (UActorComponent* Component = Cast<UActorComponent>(PrimaryObject))
	{
		FString ComponentName = Component->GetFName().ToString();
		if (ComponentName.empty()) ComponentName = Component->GetClass()->GetName();

		ImGui::SetWindowFontScale(1.5f);
		ImGui::Text("%s", ComponentName.c_str());
		ImGui::SetWindowFontScale(1.0f);
		ImGui::TextDisabled("%s", Component->GetClass()->GetName());
	}

	if (bShowDuplicateWarning)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
		ImGui::Text("Duplicate name is already in use.");
		ImGui::PopStyleColor();
	}

	if (IsValid(PrimaryActor))
	{
		RenderAddComponentMenu(PrimaryActor);
	}

	float ScrollHeight = ImGui::GetContentRegionAvail().y;
	if (ScrollHeight < 50.0f) ScrollHeight = 50.0f;

	ImGui::BeginChild("##Details", ImVec2(0, ScrollHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	{
		RenderDetails(*PrimaryTarget, SelectedTargets);
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

void FEditorPropertyWidget::RenderDetails(const FSelectionDetailTarget& PrimaryTarget, const TArray<FSelectionDetailTarget>& SelectedTargets)
{
	if (!PrimaryTarget.IsValidTarget())
	{
		ImGui::TextDisabled("Select an actor or component to view details.");
		return;
	}

	RenderTargetProperties(PrimaryTarget, SelectedTargets);
}

void FEditorPropertyWidget::CollectEditableProperties(const FSelectionDetailTarget& Target, TArray<FPropertyValue>& OutProps) const
{
	OutProps.clear();
	if (!Target.IsValidTarget())
	{
		return;
	}

	if (Target.ObjectPtr)
	{
		Target.ObjectPtr->PreGetEditableProperties();
	}

	TArray<const FProperty*> Properties;
	Target.StructType->GetPropertyRefs(Properties);
	for (const FProperty* Property : Properties)
	{
		if (!Property || (Property->Flags & PF_Edit) == 0)
		{
			continue;
		}
		if (Target.ObjectPtr && !Target.ObjectPtr->ShouldExposeProperty(*Property))
		{
			continue;
		}
		if (!Property->GetValuePtrFor(Target.ContainerPtr))
		{
			continue;
		}

		OutProps.push_back(Property->ToValue(Target.ContainerPtr, Target.ObjectPtr));
	}
}

void FEditorPropertyWidget::RenderTargetProperties(const FSelectionDetailTarget& PrimaryTarget, const TArray<FSelectionDetailTarget>& SelectedTargets)
{
	TArray<FPropertyValue> Props;
	CollectEditableProperties(PrimaryTarget, Props);

	UActorComponent* SelectedComponent = Cast<UActorComponent>(PrimaryTarget.ObjectPtr);
	AActor* OwnerActor = SelectedComponent ? SelectedComponent->GetOwner() : Cast<AActor>(PrimaryTarget.ObjectPtr);
	if (IsValid(SelectedComponent) && IsValid(OwnerActor) && SelectedComponent != OwnerActor->GetRootComponent())
	{
		if (ImGui::Button("Remove"))
		{
			OwnerActor->RemoveComponent(SelectedComponent);
			if (SelectionManager)
			{
				SelectionManager->SelectActorDetails(OwnerActor);
			}
			return;
		}
		ImGui::Separator();
	}

	bool bIsRootSceneComponent = false;
	if (USceneComponent* SceneComp = Cast<USceneComponent>(PrimaryTarget.ObjectPtr))
	{
		bIsRootSceneComponent = SceneComp->GetParent() == nullptr;
	}

	TArray<std::string> CategoryOrder;
	for (const FPropertyValue& Prop : Props)
	{
		const char* PropertyCategory = Prop.GetCategory();
		bool bFound = false;
		for (const std::string& Category : CategoryOrder)
		{
			if (Category == PropertyCategory)
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			CategoryOrder.push_back(PropertyCategory);
		}
	}

	bool bAnyChanged = false;
	for (const std::string& Category : CategoryOrder)
	{
		if (bIsRootSceneComponent && Category == "Transform")
		{
			continue;
		}

		if (!Category.empty())
		{
			const bool bOpen = ImGui::CollapsingHeader(Category.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
			if (!bOpen)
			{
				continue;
			}
		}

		if (Category == "Transform")
		{
			if (RenderTransformCategory(Props, Category, SelectedTargets))
			{
				bAnyChanged = true;
			}
			continue;
		}

		if (ImGui::BeginTable("##PropertyTable", 2,
			ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_PadOuterX))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 150.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			for (int32 i = 0; i < static_cast<int32>(Props.size()); ++i)
			{
				if (Category != Props[i].GetCategory())
				{
					continue;
				}

				ImGui::TableNextRow();
				ImGui::PushID(i);

				ImGui::TableSetColumnIndex(0);
				ImGui::SetWindowFontScale(0.92f);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(GetPropertyDisplayName(Props[i]));
				ImGui::SetWindowFontScale(1.0f);

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1);

				const bool bChanged = RenderPropertyWidget(Props, i);
				if (bChanged)
				{
					bAnyChanged = true;
					PropagatePropertyChange(Props[i], SelectedTargets);
				}
				ImGui::PopID();
			}

			ImGui::EndTable();
		}
	}

	if (PrimaryTarget.ObjectPtr)
	{
		RenderCallInEditorFunctions(PrimaryTarget.ObjectPtr);
	}

	if (bAnyChanged && IsValid(SelectedComponent) && SelectedComponent->IsA<USceneComponent>())
	{
		static_cast<USceneComponent*>(SelectedComponent)->MarkTransformDirty();
	}
}

bool FEditorPropertyWidget::RenderTransformCategory(
	TArray<FPropertyValue>& Props,
	const std::string& Category,
	const TArray<FSelectionDetailTarget>& SelectedTargets)
{
	bool bAnyChanged = false;

	if (ImGui::BeginTable("##TransformPropertyTable", 4,
		ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 150.0f);
		ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthStretch);

		for (int32 i = 0; i < static_cast<int32>(Props.size()); ++i)
		{
			FPropertyValue& Prop = Props[i];
			if (Category != Prop.GetCategory())
			{
				continue;
			}

			const bool bScaleRow = std::strcmp(GetPropertyDisplayName(Prop), "Scale") == 0;
			ImGui::PushID(i);
			const bool bChanged = RenderTransformVectorRow(Prop, bScaleRow);
			if (bChanged)
			{
				bAnyChanged = true;
				PropagatePropertyChange(Prop, SelectedTargets);
			}
			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	return bAnyChanged;
}

bool FEditorPropertyWidget::RenderTransformVectorRow(FPropertyValue& Prop, bool bScaleRow)
{
	if (Prop.GetType() != EPropertyType::Vec3 && Prop.GetType() != EPropertyType::Rotator)
	{
		return false;
	}

	bool bReadOnly = HasTruthyPropertyMetadata(Prop, "readonly");
	if (bReadOnly)
	{
		ImGui::BeginDisabled();
	}

	float Values[3] = { 0.0f, 0.0f, 0.0f };
	if (Prop.GetType() == EPropertyType::Rotator)
	{
		FRotator* Rot = static_cast<FRotator*>(Prop.GetValuePtr());
		if (!Rot)
		{
			if (bReadOnly)
			{
				ImGui::EndDisabled();
			}
			return false;
		}
		Values[0] = Rot->Roll;
		Values[1] = Rot->Pitch;
		Values[2] = Rot->Yaw;
	}
	else
	{
		float* Vec = static_cast<float*>(Prop.GetValuePtr());
		if (!Vec)
		{
			if (bReadOnly)
			{
				ImGui::EndDisabled();
			}
			return false;
		}
		Values[0] = Vec[0];
		Values[1] = Vec[1];
		Values[2] = Vec[2];
	}

	const float OldValues[3] = { Values[0], Values[1], Values[2] };
	int32 ChangedAxis = -1;

	ImGui::TableNextRow();

	ImGui::TableSetColumnIndex(0);
	ImGui::SetWindowFontScale(0.92f);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(GetPropertyDisplayName(Prop));
	ImGui::SetWindowFontScale(1.0f);

	if (bScaleRow)
	{
		ImGui::SameLine();
		DrawTransformScaleLinkToggle();
	}

	const char* AxisLabels[3] = { "X", "Y", "Z" };
	const char* AxisIds[3] = { "##X", "##Y", "##Z" };
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		ImGui::TableSetColumnIndex(Axis + 1);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(AxisLabels[Axis]);
		ImGui::SameLine(0.0f, 4.0f);
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::DragFloat(AxisIds[Axis], &Values[Axis], Prop.GetSpeed(), 0.0f, 0.0f, "%.3f"))
		{
			ChangedAxis = Axis;
		}
	}

	if (ChangedAxis >= 0 && bScaleRow && bTransformScaleLinked)
	{
		const float OldAxisValue = OldValues[ChangedAxis];
		const float NewAxisValue = Values[ChangedAxis];
		if (std::fabs(OldAxisValue) > 0.000001f)
		{
			const float ScaleRatio = NewAxisValue / OldAxisValue;
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				if (Axis != ChangedAxis)
				{
					Values[Axis] = OldValues[Axis] * ScaleRatio;
				}
			}
		}
		else
		{
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				if (Axis != ChangedAxis)
				{
					Values[Axis] = NewAxisValue;
				}
			}
		}
	}

	const bool bChanged = ChangedAxis >= 0 && !bReadOnly;
	if (bChanged)
	{
		if (Prop.GetType() == EPropertyType::Rotator)
		{
			FRotator* Rot = static_cast<FRotator*>(Prop.GetValuePtr());
			Rot->Roll = Values[0];
			Rot->Pitch = Values[1];
			Rot->Yaw = Values[2];
			if (USceneComponent* SceneComponent = Cast<USceneComponent>(Prop.Object))
			{
				SceneComponent->ApplyCachedEditRotator();
			}
		}
		else
		{
			float* Vec = static_cast<float*>(Prop.GetValuePtr());
			Vec[0] = Values[0];
			Vec[1] = Values[1];
			Vec[2] = Values[2];
		}

		DispatchPostEditChange(Prop);
	}

	if (bReadOnly)
	{
		ImGui::EndDisabled();
	}

	return bChanged;
}

bool FEditorPropertyWidget::DrawTransformScaleLinkToggle()
{
	const float Size = ImGui::GetFrameHeight();
	const bool bClicked = ImGui::InvisibleButton("##ScaleLink", ImVec2(Size, Size));
	if (bClicked)
	{
		bTransformScaleLinked = !bTransformScaleLinked;
	}

	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImU32 Color = ImGui::GetColorU32(bTransformScaleLinked ? ImGuiCol_Text : ImGuiCol_TextDisabled);
	const float MidY = (Min.y + Max.y) * 0.5f;
	const float Pad = Size * 0.23f;
	const float LinkW = Size * 0.32f;
	const float LinkH = Size * 0.24f;

	ImVec2 LeftMin(Min.x + Pad, MidY - LinkH);
	ImVec2 LeftMax(Min.x + Pad + LinkW, MidY + LinkH);
	ImVec2 RightMin(Max.x - Pad - LinkW, MidY - LinkH);
	ImVec2 RightMax(Max.x - Pad, MidY + LinkH);
	DrawList->AddRect(LeftMin, LeftMax, Color, LinkH, 0, 1.5f);
	DrawList->AddRect(RightMin, RightMax, Color, LinkH, 0, 1.5f);
	DrawList->AddLine(ImVec2(LeftMax.x - 1.0f, MidY), ImVec2(RightMin.x + 1.0f, MidY), Color, 1.5f);

	if (!bTransformScaleLinked)
	{
		DrawList->AddLine(ImVec2(Min.x + Pad, Max.y - Pad), ImVec2(Max.x - Pad, Min.y + Pad), Color, 1.5f);
	}

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(bTransformScaleLinked ? "Unlock scale axes" : "Link scale axes");
	}

	return bClicked;
}

void FEditorPropertyWidget::RenderCallInEditorFunctions(UObject* Object)
{
	if (!Object || !Object->GetClass())
	{
		return;
	}

	TArray<const FFunction*> Functions;
	Object->GetClass()->GetFunctionRefs(Functions);

	TArray<const FFunction*> CallInEditorFunctions;
	for (const FFunction* Function : Functions)
	{
		if (Function && Function->HasAnyFunctionFlags(FUNC_CallInEditor) && !Function->IsStatic())
		{
			CallInEditorFunctions.push_back(Function);
		}
	}

	if (CallInEditorFunctions.empty())
	{
		return;
	}

	ImGui::Spacing();
	ImGui::Separator();
	if (!ImGui::CollapsingHeader("Functions", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	for (const FFunction* Function : CallInEditorFunctions)
	{
		if (!Function)
		{
			continue;
		}

		FString DisabledReason;
		bool    bCallableWithoutInput = true;
		for (const FProperty* Parameter : Function->GetParameters())
		{
			if (!Parameter)
			{
				continue;
			}
			const bool bOutOnly    = (Parameter->Flags & PF_OutParm) != 0 && (Parameter->Flags & PF_ConstParm) == 0;
			const bool bHasDefault = Parameter->Metadata.find("defaultvalue") != Parameter->Metadata.end();
			if (!bOutOnly && !bHasDefault)
			{
				bCallableWithoutInput = false;
				DisabledReason        = FString("Requires parameter: ") + (Parameter->Name ? Parameter->Name : "");
				break;
			}
		}

		ImGui::PushID(Function->GetSignature());
		if (!bCallableWithoutInput)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button(Function->GetDisplayName(), ImVec2(-1.0f, 0.0f)) && bCallableWithoutInput)
		{
			void* Storage = Function->CreateParameterStorage();
			if (!Storage)
			{
				UE_LOG("[CallInEditor] Failed to allocate parameter storage: %s", Function->GetSignature());
			}
			else
			{
				const bool bOk = Function->Invoke(Object, Storage, nullptr);
				if (!bOk)
				{
					UE_LOG("[CallInEditor] Native invoke failed: %s", Function->GetSignature());
				}
				Function->DestroyParameterStorage(Storage);
			}
		}

		if (!bCallableWithoutInput)
		{
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", DisabledReason.c_str());
			}
		}
		else if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", Function->GetSignature());
		}

		ImGui::PopID();
	}
}

void FEditorPropertyWidget::RenderAddComponentMenu(AActor* Actor)
{
	if (!IsValid(Actor)) return;

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

}

void FEditorPropertyWidget::PropagatePropertyChange(const FPropertyValue& SourceProp, const TArray<FSelectionDetailTarget>& SelectedTargets)
{
	if (!SourceProp.Property || SelectedTargets.size() < 2)
	{
		return;
	}

	for (size_t TargetIndex = 1; TargetIndex < SelectedTargets.size(); ++TargetIndex)
	{
		const FSelectionDetailTarget& Target = SelectedTargets[TargetIndex];
		if (!Target.IsValidTarget())
		{
			continue;
		}
		if (SourceProp.Object && Target.StructType != SourceProp.Object->GetClass())
		{
			continue;
		}

		TArray<const FProperty*> Properties;
		Target.StructType->GetPropertyRefs(Properties);
		const FProperty* DstProperty = nullptr;
		for (const FProperty* Property : Properties)
		{
			if (Property && Property->Name && std::strcmp(SourceProp.GetName(), Property->Name) == 0 && Property->GetType() == SourceProp.GetType())
			{
				DstProperty = Property;
				break;
			}
		}

		if (!DstProperty || !DstProperty->GetValuePtrFor(Target.ContainerPtr))
		{
			continue;
		}

		FPropertyValue DstProp = DstProperty->ToValue(Target.ContainerPtr, Target.ObjectPtr);
		if (CopyPropertyValue(SourceProp, DstProp))
		{
			DispatchPostEditChange(DstProp);
			if (USceneComponent* SceneComponent = Cast<USceneComponent>(Target.ObjectPtr))
			{
				SceneComponent->MarkTransformDirty();
			}
		}
	}
}
void FEditorPropertyWidget::AddComponentToActor(AActor* Actor, UClass* ComponentClass)
{
	if (!IsValid(Actor) || !ComponentClass) return;

	UActorComponent* Comp = Actor->AddComponentByClass(ComponentClass);
	if (!Comp) return;
	UActorComponent* SelectedComponent = SelectionManager ? SelectionManager->GetSelectedActorComponent() : nullptr;

	if (ComponentClass->IsA(USceneComponent::StaticClass()))
	{
		USceneComponent* Root = Actor->GetRootComponent();
		USceneComponent* SceneComp = Cast<USceneComponent>(Comp);

		if (IsValid(SelectedComponent) && SelectedComponent->IsA<USceneComponent>())
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

	if (SelectionManager)
	{
		SelectionManager->SelectActorComponent(Comp);
	}
}

bool FEditorPropertyWidget::RenderSoftObjectPropertyWidget(FPropertyValue& Prop)
{
	bool bChanged = false;
	void* ValuePtr = Prop.GetValuePtr();
	if (!ValuePtr)
	{
		return false;
	}

	const FSoftObjectProperty* SoftProperty = Prop.Property ? Prop.Property->AsSoftObjectProperty() : nullptr;
	FString AssetType = SoftProperty ? SoftProperty->GetAssetType() : GetAssetTypeMetadata(Prop);
	FString* Val = SoftProperty ? nullptr : static_cast<FString*>(ValuePtr);
	FString CurrentPath = SoftProperty ? SoftProperty->GetPath(Prop.ContainerPtr) : *Val;
	auto SetPath = [&](const FString& NewPath)
	{
		if (SoftProperty)
		{
			SoftProperty->SetPath(Prop.ContainerPtr, NewPath);
		}
		else
		{
			*Val = NewPath;
		}
		CurrentPath = NewPath;
	};

	if (AssetType == "Material")
	{
		FString Preview = (CurrentPath.empty() || CurrentPath == "None") ? "None" : CurrentPath;
		if (ImGui::BeginCombo("##Material", Preview.c_str()))
		{
			bool bSelectedNone = (CurrentPath == "None" || CurrentPath.empty());
			if (ImGui::Selectable("None", bSelectedNone))
			{
				SetPath("None");
				bChanged = true;
			}
			if (bSelectedNone) ImGui::SetItemDefaultFocus();

			const TArray<FMaterialAssetListItem>& MatFiles = FMaterialManager::Get().GetAvailableMaterialFiles();
			for (const FMaterialAssetListItem& Item : MatFiles)
			{
				bool bSelected = (CurrentPath == Item.FullPath);
				if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
				{
					SetPath(Item.FullPath);
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
				SetPath(FPaths::ToUtf8(
					ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring()
				));
				bChanged = true;
			}
			ImGui::EndDragDropTarget();
		}
		return bChanged;
	}

	if (AssetType == "Script")
	{
		char Buf[256];
		strncpy_s(Buf, sizeof(Buf), CurrentPath.c_str(), _TRUNCATE);
		if (ImGui::InputText("##Value", Buf, sizeof(Buf)))
		{
			SetPath(Buf);
			bChanged = true;
		}

		if (ImGui::Button("Edit Script"))
		{
			if (!FLuaScriptManager::OpenOrCreateScript(CurrentPath))
			{
				UE_LOG("Failed to open script file: %s", CurrentPath.c_str());
			}
		}
		return bChanged;
	}

	if (AssetType == "SkeletalMesh")
	{
		FString Preview = CurrentPath.empty() ? "None" : GetStemFromPath(CurrentPath);
		if (CurrentPath == "None") Preview = "None";

		float ButtonWidth = ImGui::CalcTextSize("Import FBX").x + ImGui::GetStyle().FramePadding.x * 2.0f;
		float Spacing = ImGui::GetStyle().ItemSpacing.x;
		ImGui::SetNextItemWidth(-(ButtonWidth + Spacing));
		if (ImGui::BeginCombo("##SkeletalMesh", Preview.c_str()))
		{
			bool bSelectedNone = (CurrentPath == "None");
			if (ImGui::Selectable("None", bSelectedNone))
			{
				SetPath("None");
				bChanged = true;
			}
			if (bSelectedNone)
				ImGui::SetItemDefaultFocus();
			const TArray<FAssetListItem>& MeshFiles = FMeshManager::GetAvailableSkeletalMeshFiles();
			for (const FAssetListItem& Item : MeshFiles)
			{
				bool bSelected = (CurrentPath == Item.FullPath);
				if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
				{
					SetPath(Item.FullPath);
					bChanged = true;
				}
				if (bSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();

		ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ButtonWidth);
		if (ImGui::Button("Import FBX"))
		{
			FString FbxPath = OpenFbxFileDialog();
			if (!FbxPath.empty())
			{
				FFbxImportOptionsDialog::BeginSceneImport(SkeletalFbxImportDialog, FbxPath);
			}
		}

		FFbxSceneImportRequest       Request;
		const EFbxImportDialogResult DialogResult = FFbxImportOptionsDialog::RenderSceneImportPopup(
			"Skeletal FBX Import Options",
			SkeletalFbxImportDialog,
			Request
		);
		if (DialogResult == EFbxImportDialogResult::Submitted)
		{
			FFbxSceneImportResult Result;
			const auto ImportStart = std::chrono::steady_clock::now();
			if (FMeshManager::ImportFbxScene(Request, GEngine->GetRenderer().GetFD3DDevice().GetDevice(), Result))
			{
				if (Result.SkeletalMesh)
				{
					const std::chrono::duration<double> Elapsed = std::chrono::steady_clock::now() - ImportStart;
					FMeshEditorWidget::RecordImportDurationForAsset(
						Result.SkeletalMesh->GetAssetPathFileName(),
						Elapsed.count()
					);
					SetPath(Result.SkeletalMesh->GetAssetPathFileName());
					bChanged = true;
				}
				FMeshManager::ScanMeshAssets();
				FFbxImportOptionsDialog::RequestClose(SkeletalFbxImportDialog);
			}
			else
			{
				SkeletalFbxImportDialog.Error = "FBX import failed. See the engine log for details.";
			}
		}

		return bChanged;
	}

	if (AssetType == "UAnimSequence")
	{
		FString Preview = CurrentPath.empty() ? "None" : GetStemFromPath(CurrentPath);
		if (CurrentPath == "None") Preview = "None";

		if (ImGui::BeginCombo("##AnimSequence", Preview.c_str()))
		{
			bool bSelectedNone = (CurrentPath == "None" || CurrentPath.empty());
			if (ImGui::Selectable("None", bSelectedNone))
			{
				SetPath("None");
				bChanged = true;
			}
			if (bSelectedNone)
			{
				ImGui::SetItemDefaultFocus();
			}

			const TArray<FAssetListItem>& AnimFiles = FAssetRegistry::ListByTypeName("UAnimSequence");
			for (const FAssetListItem& Item : AnimFiles)
			{
				bool bSelected = (CurrentPath == Item.FullPath);
				if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
				{
					SetPath(Item.FullPath);
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

	if (AssetType == "UAnimGraphAsset")
	{
		FString Preview = CurrentPath.empty() ? "None" : GetStemFromPath(CurrentPath);
		if (CurrentPath == "None") Preview = "None";

		if (ImGui::BeginCombo("##AnimGraphAsset", Preview.c_str()))
		{
			bool bSelectedNone = (CurrentPath == "None" || CurrentPath.empty());
			if (ImGui::Selectable("None", bSelectedNone))
			{
				SetPath("None");
				bChanged = true;
			}
			if (bSelectedNone)
			{
				ImGui::SetItemDefaultFocus();
			}

			const TArray<FAssetListItem>& GraphFiles = FAssetRegistry::ListByTypeName("UAnimGraphAsset");
			for (const FAssetListItem& Item : GraphFiles)
			{
				bool bSelected = (CurrentPath == Item.FullPath);
				if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
				{
					SetPath(Item.FullPath);
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

	if (AssetType == "LuaAnimScript")
	{
		// 콤보 + "Edit Script" 버튼 하이브리드 — Content/Script/Anim 하위 .lua 선택 + 즉시 편집.
		// 리스트는 FAssetRegistry::ListByTypeName 가 매 호출 시 디렉토리 스캔 (콤보 열 때만).
		FString Preview = (CurrentPath.empty() || CurrentPath == "None") ? "None" : GetStemFromPath(CurrentPath);

		float ButtonWidth = ImGui::CalcTextSize("Edit Script").x + ImGui::GetStyle().FramePadding.x * 2.0f;
		float Spacing     = ImGui::GetStyle().ItemSpacing.x;
		ImGui::SetNextItemWidth(-(ButtonWidth + Spacing));

		if (ImGui::BeginCombo("##LuaAnimScript", Preview.c_str()))
		{
			bool bSelectedNone = (CurrentPath == "None" || CurrentPath.empty());
			if (ImGui::Selectable("None", bSelectedNone))
			{
				SetPath("None");
				bChanged = true;
			}
			if (bSelectedNone) ImGui::SetItemDefaultFocus();

			const TArray<FAssetListItem>& LuaFiles = FAssetRegistry::ListByTypeName("LuaAnimScript");
			for (const FAssetListItem& Item : LuaFiles)
			{
				bool bSelected = (CurrentPath == Item.FullPath);
				if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
				{
					SetPath(Item.FullPath);
					bChanged = true;
				}
				if (bSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		if (ImGui::Button("Edit Script"))
		{
			if (!FLuaScriptManager::OpenOrCreateScript(CurrentPath))
			{
				UE_LOG("Failed to open script file: %s", CurrentPath.c_str());
			}
		}
		return bChanged;
	}

	if (AssetType == "ULuaBlueprintAsset")
	{
		FString Preview = (CurrentPath.empty() || CurrentPath == "None") ? "None" : GetStemFromPath(CurrentPath);

		if (ImGui::BeginCombo("##LuaBlueprintAsset", Preview.c_str()))
		{
			const bool bSelectedNone = (CurrentPath == "None" || CurrentPath.empty());
			if (ImGui::Selectable("None", bSelectedNone))
			{
				SetPath("None");
				bChanged = true;
			}
			if (bSelectedNone) ImGui::SetItemDefaultFocus();

			const TArray<FAssetListItem>& BlueprintFiles = FAssetRegistry::ListByTypeName("ULuaBlueprintAsset");
			for (const FAssetListItem& Item : BlueprintFiles)
			{
				const bool bSelected = (CurrentPath == Item.FullPath);
				if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
				{
					SetPath(Item.FullPath);
					bChanged = true;
				}
				if (bSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		return bChanged;
	}

	if (AssetType == "UParticleSystem")
	{
		FString Preview = (CurrentPath.empty() || CurrentPath == "None") ? "None" : GetStemFromPath(CurrentPath);

		if (ImGui::BeginCombo("##ParticleSystem", Preview.c_str()))
		{
			const bool bSelectedNone = (CurrentPath == "None" || CurrentPath.empty());

			if (ImGui::Selectable("None", bSelectedNone))
			{
				SetPath("None");
				bChanged = true;
			}

			if (bSelectedNone)
			{
				ImGui::SetItemDefaultFocus();
			}

			const TArray<FAssetListItem>& ParticleFiles = FAssetRegistry::ListByTypeName("UParticleSystem");

			for (const FAssetListItem& Item : ParticleFiles)
			{
				const bool bSelected = (CurrentPath == Item.FullPath);

				if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
				{
					SetPath(Item.FullPath);
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

	FString Preview = CurrentPath.empty() ? "None" : GetStemFromPath(CurrentPath);
	if (CurrentPath == "None") Preview = "None";

	float ButtonWidth = ImGui::CalcTextSize("Import").x + ImGui::GetStyle().FramePadding.x * 2.0f;
	float Spacing = ImGui::GetStyle().ItemSpacing.x;
	ImGui::SetNextItemWidth(-(ButtonWidth + Spacing));

	if (ImGui::BeginCombo("##Mesh", Preview.c_str()))
	{
		bool bSelectedNone = (CurrentPath == "None");
		if (ImGui::Selectable("None", bSelectedNone))
		{
			SetPath("None");
			bChanged = true;
		}
		if (bSelectedNone)
			ImGui::SetItemDefaultFocus();

		const TArray<FAssetListItem>& MeshFiles = FMeshManager::GetAvailableStaticMeshFiles();
		for (const FAssetListItem& Item : MeshFiles)
		{
			bool bSelected = (CurrentPath == Item.FullPath);
			if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
			{
				SetPath(Item.FullPath);
				bChanged = true;
			}
			if (bSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();

	ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ButtonWidth);
	if (ImGui::Button("Import"))
	{
		FString MeshPath = OpenStaticMeshFileDialog();
		if (!MeshPath.empty())
		{
			if (IsFbxFilePath(MeshPath))
			{
				PendingStaticMeshImportPath = MeshPath;
				PendingStaticMeshImportTarget = Val;
				PendingStaticFbxSkinnedMeshPolicy =
					FImportOptions::Default().StaticFbxSkinnedMeshPolicy == EStaticFbxSkinnedMeshPolicy::ImportBindPoseAsStatic ? 1 : 0;
				ImGui::OpenPopup("Static FBX Import Options");
			}
			else
			{
				ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
				UStaticMesh* Loaded = FMeshManager::LoadStaticMesh(MeshPath, Device);
				if (Loaded)
				{
					SetPath(FMeshManager::GetStaticMeshBinaryFilePath(MeshPath));
					bChanged = true;
				}
			}
		}
	}

	if (ImGui::BeginPopupModal("Static FBX Import Options", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Skinned mesh handling");
		ImGui::RadioButton("Skip skinned meshes", &PendingStaticFbxSkinnedMeshPolicy, 0);
		ImGui::RadioButton("Import bind pose as static mesh", &PendingStaticFbxSkinnedMeshPolicy, 1);

		if (ImGui::Button("Import"))
		{
			FImportOptions Options = FImportOptions::Default();
			Options.StaticFbxSkinnedMeshPolicy = PendingStaticFbxSkinnedMeshPolicy == 1
				? EStaticFbxSkinnedMeshPolicy::ImportBindPoseAsStatic
				: EStaticFbxSkinnedMeshPolicy::Skip;

			ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
			UStaticMesh* Loaded = FMeshManager::LoadStaticMesh(PendingStaticMeshImportPath, Options, Device);
			if (Loaded && PendingStaticMeshImportTarget)
			{
				*PendingStaticMeshImportTarget = FMeshManager::GetStaticMeshBinaryFilePath(PendingStaticMeshImportPath);
				bChanged = true;
			}

			PendingStaticMeshImportPath.clear();
			PendingStaticMeshImportTarget = nullptr;
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			PendingStaticMeshImportPath.clear();
			PendingStaticMeshImportTarget = nullptr;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	
	return bChanged;
}

bool FEditorPropertyWidget::RenderEnumPropertyWidget(FPropertyValue& Prop)
{
	const FEnum* EnumType = Prop.GetEnumType();
	if (!EnumType || !EnumType->GetEntries() || EnumType->GetCount() == 0 || !Prop.GetValuePtr())
	{
		return false;
	}

	bool         bChanged  = false;
	const uint32 EnumCount = EnumType->GetCount();
	const uint32 EnumSize  = EnumType->GetSize();
	int64        Val       = 0;
	memcpy(&Val, Prop.GetValuePtr(), std::min<size_t>(EnumSize, sizeof(Val)));
	const char* Preview = EnumType->GetNameByValue(Val);
	if (ImGui::BeginCombo("##Value", Preview))
	{
		for (uint32 i = 0; i < EnumCount; ++i)
		{
			const int64 EntryValue = EnumType->GetValueAt(i);
			bool        bSelected  = (Val == EntryValue);
			if (ImGui::Selectable(EnumType->GetNameAt(i), bSelected))
			{
				memcpy(Prop.GetValuePtr(), &EntryValue, std::min<size_t>(EnumSize, sizeof(EntryValue)));
				bChanged = true;
			}
			if (bSelected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	return bChanged;
}

bool FEditorPropertyWidget::RenderStructPropertyWidget(FPropertyValue& Prop, bool bDispatchChange, const FString& PropertyPath)
{
	const FStructProperty* StructProperty = Prop.Property ? Prop.Property->AsStructProperty() : nullptr;
	if (!StructProperty || !StructProperty->GetStructType() || !Prop.GetValuePtr())
	{
		return false;
	}

	bool bChanged = false;
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_DefaultOpen |
		ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

	bool bOpen = ImGui::TreeNodeEx("##StructValue", Flags, "");
	if (bOpen)
	{
		TArray<FPropertyValue> ChildProps;
		Prop.GetStructChildren(ChildProps);

		ImGui::Indent(8.0f);

		for (int32 ci = 0; ci < (int32)ChildProps.size(); ++ci)
		{
			ImGui::PushID(ci);

			FPropertyValue& ChildProp = ChildProps[ci];
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(GetPropertyDisplayName(ChildProp));
			ImGui::SameLine(120.0f);
			ImGui::SetNextItemWidth(-1);

			const FString ChildPath = MakePropertyPath(PropertyPath, ChildProp.GetName());
			int32 ChildIdx = ci;
			if (RenderPropertyWidget(ChildProps, ChildIdx, bDispatchChange, ChildPath))
			{
				bChanged = true;
			}
			ImGui::PopID();
		}

		ImGui::Unindent(8.0f);
		ImGui::TreePop();
	}

	return bChanged;
}

bool FEditorPropertyWidget::RenderArrayPropertyWidget(FPropertyValue& Prop, bool bDispatchChange, const FString& PropertyPath)
{
	const FArrayProperty* ArrayProperty = Prop.Property ? Prop.Property->AsArrayProperty() : nullptr;
	void* ArrayPtr = Prop.GetValuePtr();
	if (!ArrayProperty || !ArrayPtr || !ArrayProperty->GetArrayOps() || !ArrayProperty->GetInnerProperty())
	{
		return false;
	}

	const FArrayProperty::FArrayOps* Ops = ArrayProperty->GetArrayOps();
	const FProperty* InnerProperty = ArrayProperty->GetInnerProperty();
	if (!Ops->GetNum || !Ops->GetElementPtr)
	{
		return false;
	}

	bool bChanged = false;
	size_t Num = Ops->GetNum(ArrayPtr);
	const bool bEditFixedSize = HasTruthyPropertyMetadata(Prop, "editfixedsize") || HasTruthyPropertyMetadata(Prop, "fixedsize");

	if (!bEditFixedSize && Ops->InsertDefault && ImGui::Button("+"))
	{
		Ops->InsertDefault(ArrayPtr, Num);
		bChanged = true;
		if (bDispatchChange)
		{
			DispatchPostEditChange(Prop, EPropertyChangeType::ArrayAdd, static_cast<int32>(Num), MakeArrayElementPath(PropertyPath, static_cast<int32>(Num)));
		}
		Num = Ops->GetNum(ArrayPtr);
	}

	for (int32 ElemIdx = 0; ElemIdx < static_cast<int32>(Num); ++ElemIdx)
	{
		void* ElementPtr = Ops->GetElementPtr(ArrayPtr, static_cast<size_t>(ElemIdx));
		if (!ElementPtr)
		{
			continue;
		}

		ImGui::PushID(ElemIdx);

		FString ElementName = "Element " + std::to_string(ElemIdx);
		const FString ElementPath = MakeArrayElementPath(PropertyPath, ElemIdx);

		if (!bEditFixedSize && Ops->RemoveAt && ImGui::Button("-"))
		{
			Ops->RemoveAt(ArrayPtr, static_cast<size_t>(ElemIdx));
			bChanged = true;
			if (bDispatchChange)
			{
				DispatchPostEditChange(Prop, EPropertyChangeType::ArrayRemove, ElemIdx, ElementPath, ElementName.c_str(), ElementName.c_str());
			}
			ImGui::PopID();
			break;
		}

		if (!bEditFixedSize && Ops->RemoveAt)
		{
			ImGui::SameLine();
		}
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(ElementName.c_str());
		ImGui::SameLine(120.0f);
		ImGui::SetNextItemWidth(-1);

		FPropertyValue ElementValue;
		ElementValue.Object = Prop.Object;
		ElementValue.Property = InnerProperty;
		ElementValue.ContainerPtr = ElementPtr;

		TArray<FPropertyValue> ElementProps;
		ElementProps.push_back(ElementValue);
		int32 ElementPropIndex = 0;
		if (RenderPropertyWidget(ElementProps, ElementPropIndex, false, ElementPath))
		{
			bChanged = true;
			if (bDispatchChange)
			{
				DispatchPostEditChange(Prop, EPropertyChangeType::ValueSet, ElemIdx, ElementPath, ElementName.c_str(), ElementName.c_str());
			}
		}

		ImGui::PopID();
	}

	return bChanged;
}

bool FEditorPropertyWidget::RenderPropertyWidget(TArray<FPropertyValue>& Props, int32& Index, bool bDispatchChange, const FString& PropertyPath)
{
	ImGui::PushID(Index);
	FPropertyValue& Prop = Props[Index];
	bool bChanged = false;
	const FString EffectivePropertyPath = PropertyPath.empty() ? FString(Prop.GetName()) : PropertyPath;
	const bool bReadOnly = Prop.Property && (Prop.Property->Flags & PF_ReadOnly) != 0;
	if (bReadOnly)
	{
		ImGui::BeginDisabled();
	}

	switch (Prop.GetType())
	{
	case EPropertyType::Bool:
	{
		bool* Val = static_cast<bool*>(Prop.GetValuePtr());
		if (!Val)
		{
			break;
		}

		bChanged = ImGui::Checkbox("##Value", Val);

		break;
	}
	case EPropertyType::ByteBool:
	{
		uint8* Val = static_cast<uint8*>(Prop.GetValuePtr());
		if (!Val)
		{
			break;
		}

		bool bVal = (*Val != 0);
		if (ImGui::Checkbox("##Value", &bVal))
		{
			*Val = bVal ? 1 : 0;
			bChanged = true;
		}

		break;
	}
	case EPropertyType::Int:
	{
		const FNumericProperty* NumericProperty = Prop.Property ? Prop.Property->AsNumericProperty() : nullptr;
		int32* Val = static_cast<int32*>(Prop.GetValuePtr());
		const float Min = NumericProperty ? NumericProperty->GetMin() : Prop.GetMin();
		const float Max = NumericProperty ? NumericProperty->GetMax() : Prop.GetMax();
		const float Speed = NumericProperty ? NumericProperty->GetSpeed() : Prop.GetSpeed();
		if (Min != 0.0f || Max != 0.0f)
			bChanged = ImGui::DragInt("##Value", Val, Speed, (int32)Min, (int32)Max);
		else
			bChanged = ImGui::DragInt("##Value", Val, Speed);
		break;
	}
	case EPropertyType::Float:
	{
		const FNumericProperty* NumericProperty = Prop.Property ? Prop.Property->AsNumericProperty() : nullptr;
		float* Val = static_cast<float*>(Prop.GetValuePtr());
		const float Min = NumericProperty ? NumericProperty->GetMin() : Prop.GetMin();
		const float Max = NumericProperty ? NumericProperty->GetMax() : Prop.GetMax();
		const float Speed = NumericProperty ? NumericProperty->GetSpeed() : Prop.GetSpeed();
		if (Min != 0.0f || Max != 0.0f)
			bChanged = ImGui::DragFloat("##Value", Val, Speed, Min, Max, "%.4f");
		else
			bChanged = ImGui::DragFloat("##Value", Val, Speed);
		break;
	}
	case EPropertyType::Vec3:
	{
		float* Val = static_cast<float*>(Prop.GetValuePtr());
		bChanged = ImGui::DragFloat3("##Value", Val, Prop.GetSpeed());
		break;
	}
	case EPropertyType::Rotator:
	{
		// FRotator 메모리 레이아웃 [Pitch,Yaw,Roll] → UI X=Roll(X축), Y=Pitch(Y축), Z=Yaw(Z축)
		FRotator* Rot = static_cast<FRotator*>(Prop.GetValuePtr());
		float RotXYZ[3] = { Rot->Roll, Rot->Pitch, Rot->Yaw };
		bChanged = ImGui::DragFloat3("##Value", RotXYZ, Prop.GetSpeed());
		if (bChanged)
		{
			Rot->Roll = RotXYZ[0];
			Rot->Pitch = RotXYZ[1];
			Rot->Yaw = RotXYZ[2];
			if (USceneComponent* SceneComponent = Cast<USceneComponent>(Prop.Object))
			{
				SceneComponent->ApplyCachedEditRotator();
			}
		}
		break;
	}
	case EPropertyType::Vec4:
	{
		float* Val = static_cast<float*>(Prop.GetValuePtr());
		bChanged = ImGui::DragFloat4("##Value", Val, Prop.GetSpeed());
		break;
	}
	case EPropertyType::Color4:
	{
		float* Val = static_cast<float*>(Prop.GetValuePtr());
		bChanged = ImGui::ColorEdit4("##Value", Val);
		break;
	}
	case EPropertyType::String:
	{
		FString* Val = static_cast<FString*>(Prop.GetValuePtr());
		if (!Val)
		{
			break;
		}

		char Buf[256];
		strncpy_s(Buf, sizeof(Buf), Val->c_str(), _TRUNCATE);
		if (ImGui::InputText("##Value", Buf, sizeof(Buf)))
		{
			*Val = Buf;
			bChanged = true;
		}
		break;
	}
	case EPropertyType::ClassRef:
	{
		bChanged = RenderClassPropertyWidget(Prop);
		break;
	}
	case EPropertyType::ObjectRef:
	{
		const FObjectProperty* ObjectValueProperty = Prop.Property ? Prop.Property->AsObjectProperty() : nullptr;
		if (!ObjectValueProperty)
		{
			break;
		}

		auto SetObjectValue = [&](UObject* Object)
				{
				ObjectValueProperty->SetObjectValue(Prop.ContainerPtr, Object);
				bChanged = true;
			};

		UObject* Current = ObjectValueProperty->GetObjectValue(Prop.ContainerPtr);
		FString Preview = Current ? Current->GetName() : FString("None");

		const FObjectPropertyBase* ObjectProperty = Prop.Property ? Prop.Property->AsObjectPropertyBase() : nullptr;
		UClass* AllowedClass = ObjectProperty ? ObjectProperty->GetAllowedClassType() : nullptr;

		if (AllowedClass == UStaticMesh::StaticClass())
		{
			UStaticMesh* CurrentMesh = Cast<UStaticMesh>(Current);
			Preview = CurrentMesh && CurrentMesh->GetAssetPathFileName() != "None"
				? GetStemFromPath(CurrentMesh->GetAssetPathFileName())
				: FString("None");

			float ButtonWidth = ImGui::CalcTextSize("Import").x + ImGui::GetStyle().FramePadding.x * 2.0f;
			float Spacing = ImGui::GetStyle().ItemSpacing.x;
			ImGui::SetNextItemWidth(-(ButtonWidth + Spacing));

			if (ImGui::BeginCombo("##StaticMeshObject", Preview.c_str()))
			{
				const bool bSelectedNone = CurrentMesh == nullptr;
				if (ImGui::Selectable("None", bSelectedNone))
				{
					SetObjectValue(nullptr);
				}
				if (bSelectedNone)
				{
					ImGui::SetItemDefaultFocus();
				}

				const TArray<FAssetListItem>& MeshFiles = FMeshManager::GetAvailableStaticMeshFiles();
				for (const FAssetListItem& Item : MeshFiles)
				{
					const bool bSelected = CurrentMesh && CurrentMesh->GetAssetPathFileName() == Item.FullPath;
					if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
					{
						ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
						UStaticMesh* Loaded = FMeshManager::LoadStaticMesh(Item.FullPath, Device);
						if (Loaded)
						{
							SetObjectValue(Loaded);
						}
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ButtonWidth);
			if (ImGui::Button("Import"))
			{
				FString MeshPath = OpenStaticMeshFileDialog();
				if (!MeshPath.empty())
				{
					if (IsFbxFilePath(MeshPath))
					{
						ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
						UStaticMesh* Loaded = FMeshManager::LoadStaticMesh(MeshPath, FImportOptions::Default(), Device);
						if (Loaded)
						{
							SetObjectValue(Loaded);
						}
					}
					else
					{
						ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
						UStaticMesh* Loaded = FMeshManager::LoadStaticMesh(MeshPath, Device);
						if (Loaded)
						{
							SetObjectValue(Loaded);
						}
					}
				}
			}
			break;
		}

		if (AllowedClass == USkeletalMesh::StaticClass())
		{
			USkeletalMesh* CurrentMesh = Cast<USkeletalMesh>(Current);
			Preview = CurrentMesh && CurrentMesh->GetAssetPathFileName() != "None"
				? GetStemFromPath(CurrentMesh->GetAssetPathFileName())
				: FString("None");

			float ButtonWidth = ImGui::CalcTextSize("Import FBX").x + ImGui::GetStyle().FramePadding.x * 2.0f;
			float Spacing = ImGui::GetStyle().ItemSpacing.x;
			ImGui::SetNextItemWidth(-(ButtonWidth + Spacing));

			if (ImGui::BeginCombo("##SkeletalMeshObject", Preview.c_str()))
			{
				const bool bSelectedNone = CurrentMesh == nullptr;
				if (ImGui::Selectable("None", bSelectedNone))
				{
					SetObjectValue(nullptr);
				}
				if (bSelectedNone)
				{
					ImGui::SetItemDefaultFocus();
				}

				const TArray<FAssetListItem>& MeshFiles = FMeshManager::GetAvailableSkeletalMeshFiles();
				for (const FAssetListItem& Item : MeshFiles)
				{
					const bool bSelected = CurrentMesh && CurrentMesh->GetAssetPathFileName() == Item.FullPath;
					if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
					{
						ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
						USkeletalMesh* Loaded = FMeshManager::LoadSkeletalMesh(Item.FullPath, Device);
						if (Loaded)
						{
							SetObjectValue(Loaded);
						}
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ButtonWidth);
			if (ImGui::Button("Import FBX"))
			{
				FString FbxPath = OpenFbxFileDialog();
				if (!FbxPath.empty())
				{
					FFbxImportOptionsDialog::BeginSceneImport(SkeletalFbxImportDialog, FbxPath);
				}
			}

			FFbxSceneImportRequest       Request;
			const EFbxImportDialogResult DialogResult = FFbxImportOptionsDialog::RenderSceneImportPopup(
				"Object Skeletal FBX Import Options",
				SkeletalFbxImportDialog,
				Request
			);
			if (DialogResult == EFbxImportDialogResult::Submitted)
			{
				FFbxSceneImportResult Result;
				const auto ImportStart = std::chrono::steady_clock::now();
				if (FMeshManager::ImportFbxScene(Request, GEngine->GetRenderer().GetFD3DDevice().GetDevice(), Result))
				{
					if (Result.SkeletalMesh)
					{
						const std::chrono::duration<double> Elapsed = std::chrono::steady_clock::now() - ImportStart;
						FMeshEditorWidget::RecordImportDurationForAsset(
							Result.SkeletalMesh->GetAssetPathFileName(),
							Elapsed.count()
						);
						SetObjectValue(Result.SkeletalMesh);
					}
					FMeshManager::ScanMeshAssets();
					FFbxImportOptionsDialog::RequestClose(SkeletalFbxImportDialog);
				}
				else
				{
					SkeletalFbxImportDialog.Error = "FBX import failed. See the engine log for details.";
				}
			}

			break;
		}

		if (AllowedClass == UPhysicsAsset::StaticClass())
		{
			UPhysicsAsset* CurrentPhysicsAsset = Cast<UPhysicsAsset>(Current);
			Preview = CurrentPhysicsAsset && CurrentPhysicsAsset->GetAssetPathFileName() != "None"
				? GetStemFromPath(CurrentPhysicsAsset->GetAssetPathFileName())
				: FString("None");

			if (ImGui::BeginCombo("##PhysicsAssetObject", Preview.c_str()))
			{
				const bool bSelectedNone = CurrentPhysicsAsset == nullptr;
				if (ImGui::Selectable("None", bSelectedNone))
				{
					SetObjectValue(nullptr);
				}
				if (bSelectedNone)
				{
					ImGui::SetItemDefaultFocus();
				}

				USkeletalMesh* SourceSkeletalMesh = Cast<USkeletalMesh>(Prop.Object);
				const TArray<FAssetListItem>& PhysicsAssetFiles = FAssetRegistry::ListByTypeName("UPhysicsAsset");
				for (const FAssetListItem& Item : PhysicsAssetFiles)
				{
					const bool bSelected = CurrentPhysicsAsset &&
						CurrentPhysicsAsset->GetAssetPathFileName() == Item.FullPath;
					if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
					{
						UPhysicsAsset* Loaded = FPhysicsAssetManager::Get().Load(Item.FullPath, SourceSkeletalMesh);
						if (Loaded)
						{
							SetObjectValue(Loaded);
						}
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

		if (AllowedClass && AllowedClass->IsA(UActorComponent::StaticClass()))
		{
			Preview = GetObjectReferenceChoiceLabel(Current);

			if (ImGui::BeginCombo("##OwnerObjectRef", Preview.c_str()))
			{
				const bool bSelectedNone = Current == nullptr;
				if (ImGui::Selectable("None", bSelectedNone))
				{
					SetObjectValue(nullptr);
				}
				if (bSelectedNone)
				{
					ImGui::SetItemDefaultFocus();
				}

				for (UObject* Candidate : GetOwnerObjectReferenceChoices(Prop, AllowedClass))
				{
					const FString CandidateName = GetObjectReferenceChoiceLabel(Candidate);
					const bool bSelected = Current == Candidate;
					if (ImGui::Selectable(CandidateName.c_str(), bSelected))
					{
						SetObjectValue(Candidate);
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

		if (ImGui::BeginCombo("##Value", Preview.c_str()))
		{
			const bool bSelectedNone = Current == nullptr;
			if (ImGui::Selectable("None", bSelectedNone))
			{
				SetObjectValue(nullptr);
			}
			if (bSelectedNone)
			{
				ImGui::SetItemDefaultFocus();
			}

			for (UObject* Candidate : GUObjectArray)
			{
				if (!IsValid(Candidate))
				{
					continue;
				}

				if (AllowedClass && !Candidate->GetClass()->IsA(AllowedClass))
				{
					continue;
				}

				FString CandidateName = Candidate->GetName();
				if (CandidateName.empty())
				{
					CandidateName = Candidate->GetClass()->GetName();
				}

				const bool bSelected = Current == Candidate;
				if (ImGui::Selectable(CandidateName.c_str(), bSelected))
				{
					SetObjectValue(Candidate);
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
	case EPropertyType::SoftObjectRef:
	{
		bChanged = RenderSoftObjectPropertyWidget(Prop);
		break;
	}
	case EPropertyType::Array:
	{
		bChanged = RenderArrayPropertyWidget(Prop, bDispatchChange, EffectivePropertyPath);
		bDispatchChange = false;
		break;
	}
	case EPropertyType::Name:
	{
		FName* Val = static_cast<FName*>(Prop.GetValuePtr());
		FString Current = Val->ToString();

		// 리소스 키와 매칭되는 프로퍼티면 콤보 박스로 렌더링
		TArray<FString> Names;
		FString AssetType = GetAssetTypeMetadata(Prop);
		if (AssetType.empty())
		{
			AssetType = Prop.GetName();
		}

		if (AssetType == "Font")
			Names = FResourceManager::Get().GetFontNames();
		else if (AssetType == "Particle")
			Names = FResourceManager::Get().GetParticleNames();
		else if (AssetType == "Texture")
			Names = FResourceManager::Get().GetTextureNames();

		if (!Names.empty())
		{
			if (ImGui::BeginCombo("##Value", Current.c_str()))
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
			if (ImGui::InputText("##Value", Buf, sizeof(Buf)))
			{
				*Val = FName(Buf);
				bChanged = true;
			}
		}
		break;
	}
	case EPropertyType::Enum:
	{
		bChanged = RenderEnumPropertyWidget(Prop);
		break;
	}
	case EPropertyType::Struct:
	{
		bChanged = RenderStructPropertyWidget(Prop, bDispatchChange, EffectivePropertyPath);
		bDispatchChange = false;
		break;
	}
	}

	if (bReadOnly)
	{
		ImGui::EndDisabled();
		bChanged = false;
	}

	if (bDispatchChange && bChanged)
	{
		DispatchPostEditChange(Prop, EPropertyChangeType::ValueSet, -1, EffectivePropertyPath);
	}

	ImGui::PopID();
	return bChanged;
}
