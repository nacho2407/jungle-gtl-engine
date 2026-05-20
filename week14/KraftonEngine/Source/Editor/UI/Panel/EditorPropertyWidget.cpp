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
	bool IsFbxFilePath(const FString& Path)
	{
		std::filesystem::path FilePath(FPaths::ToWide(Path));
		std::wstring Extension = FilePath.extension().wstring();
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
		return Extension == L".fbx";
	}



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

	bool RenderClassPropertyWidget(FPropertyValue& Prop)
	{
		const FClassProperty* ClassProperty = Prop.Property ? Prop.Property->AsClassProperty() : nullptr;
		if (!ClassProperty || !Prop.GetValuePtr())
		{
			return false;
		}

		UClass* AllowedClass = GetAllowedClassMetadata(Prop);
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
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 150.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.145f, 0.145f, 0.145f, 1.0f));

			for (int32 i = 0; i < (int32)Props.size(); ++i)
			{
				ImGui::TableNextRow();
				ImGui::PushID(i);

				ImGui::TableSetColumnIndex(0);

				ImGui::SetWindowFontScale(0.92f);

				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(GetPropertyDisplayName(Props[i]));

				ImGui::SetWindowFontScale(1.0f);

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1);

				RenderPropertyWidget(Props, i);
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
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 150.0f);
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
				
				ImGui::SetWindowFontScale(0.92f);

				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(GetPropertyDisplayName(Props[i]));

				ImGui::SetWindowFontScale(1.0f);

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1);

				bool bChanged = RenderPropertyWidget(Props, i);

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
	if (!EnumType || !EnumType->GetNames() || EnumType->GetCount() == 0 || !Prop.GetValuePtr())
	{
		return false;
	}

	bool bChanged = false;
	const char** EnumNames = EnumType->GetNames();
	const uint32 EnumCount = EnumType->GetCount();
	const uint32 EnumSize = EnumType->GetSize();
	int32 Val = 0;
	memcpy(&Val, Prop.GetValuePtr(), EnumSize);
	const char* Preview = ((uint32)Val < EnumCount) ? EnumNames[Val] : "Unknown";
	if (ImGui::BeginCombo("##Value", Preview))
	{
		for (uint32 i = 0; i < EnumCount; ++i)
		{
			bool bSelected = (Val == (int32)i);
			if (ImGui::Selectable(EnumNames[i], bSelected))
			{
				int32 NewVal = (int32)i;
				memcpy(Prop.GetValuePtr(), &NewVal, EnumSize);
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

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.07f, 0.07f, 0.07f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.055f, 0.525f, 1.0f, 1.0f));

		bChanged = ImGui::Checkbox("##Value", Val);

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
		break;
	}
	case EPropertyType::ByteBool:
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.07f, 0.07f, 0.07f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.055f, 0.525f, 1.0f, 1.0f));

		uint8* Val = static_cast<uint8*>(Prop.GetValuePtr());
		bool bVal = (*Val != 0);
		if (ImGui::Checkbox("##Value", &bVal))
		{
			*Val = bVal ? 1 : 0;
			bChanged = true;
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
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
			if (SelectedComponent && SelectedComponent->IsA<USceneComponent>())
			{
				static_cast<USceneComponent*>(SelectedComponent)->ApplyCachedEditRotator();
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
