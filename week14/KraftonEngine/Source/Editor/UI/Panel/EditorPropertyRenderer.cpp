#include "Editor/UI/Panel/EditorPropertyRenderer.h"

#include "Editor/EditorEngine.h"

#include "ImGui/imgui.h"
#include "Asset/AssetRegistry.h"
#include "Component/ActorComponent.h"
#include "Component/SceneComponent.h"
#include "Core/Property/ArrayProperty.h"
#include "Core/Property/ClassProperty.h"
#include "Core/Property/NumericProperty.h"
#include "Core/Property/ObjectProperty.h"
#include "Core/Property/SoftObjectProperty.h"
#include "Core/Property/StructProperty.h"
#include "Core/Types/ClassTypes.h"
#include "Editor/UI/Asset/Mesh/MeshEditorWidget.h"
#include "Editor/UI/ContentBrowser/ContentItem.h"
#include "GameFramework/AActor.h"
#include "Lua/LuaScriptManager.h"
#include "Math/Rotator.h"
#include "Materials/MaterialManager.h"
#include "Mesh/Importer/MeshImportOptions.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Static/StaticMesh.h"
#include "Object/FName.h"
#include "Object/ObjectIterator.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Platform/Paths.h"
#include "Resource/ResourceManager.h"

#include <Windows.h>
#include <commdlg.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>

namespace
{
	constexpr float PropertyNameColumnWidth = 150.0f;
	constexpr float StructChildIndentWidth = 8.0f;

	bool IsFbxFilePath(const FString& Path)
	{
		std::filesystem::path FilePath(FPaths::ToWide(Path));
		std::wstring Extension = FilePath.extension().wstring();
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
		return Extension == L".fbx";
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
		Event.DisplayName = OverrideDisplayName ? OverrideDisplayName : FEditorPropertyRenderer::GetPropertyDisplayName(Prop);
		Event.PropertyPath = PropertyPath.empty() ? Prop.GetName() : PropertyPath;
		Event.Type = Prop.GetType();
		Event.ChangeType = ChangeType;
		Event.ArrayIndex = ArrayIndex;
		Prop.Object->PostEditChangeProperty(Event);
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

	bool BeginPropertyChildTable(const char* Id)
	{
		const ImGuiTableFlags Flags =
			ImGuiTableFlags_SizingStretchProp |
			ImGuiTableFlags_BordersInnerV |
			ImGuiTableFlags_PadOuterX |
			ImGuiTableFlags_RowBg;

		if (!ImGui::BeginTable(Id, 2, Flags))
		{
			return false;
		}

		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, PropertyNameColumnWidth);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
		ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.145f, 0.145f, 0.145f, 1.0f));
		return true;
	}

	void EndPropertyChildTable()
	{
		ImGui::EndTable();
		ImGui::PopStyleColor(2);
	}

	void DrawPropertyTableLabel(const char* Label)
	{
		ImGui::SetWindowFontScale(0.92f);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(Label ? Label : "");
		ImGui::SetWindowFontScale(1.0f);
	}
}

const char* FEditorPropertyRenderer::GetPropertyDisplayName(const FPropertyValue& Prop)
{
	return Prop.GetDisplayName();
}

FString FEditorPropertyRenderer::OpenStaticMeshFileDialog()
{
	wchar_t FileName[MAX_PATH] = L"";
	OPENFILENAMEW Ofn = {};
	Ofn.lStructSize = sizeof(Ofn);
	Ofn.hwndOwner = GetActiveWindow();
	Ofn.lpstrFile = FileName;
	Ofn.nMaxFile = MAX_PATH;
	Ofn.lpstrFilter = L"Static Mesh Files (*.obj;*.fbx)\0*.obj;*.fbx\0OBJ Files (*.obj)\0*.obj\0FBX Files (*.fbx)\0*.fbx\0All Files (*.*)\0*.*\0";
	Ofn.nFilterIndex = 1;
	Ofn.lpstrTitle = L"Import Static Mesh";
	Ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (GetOpenFileNameW(&Ofn))
	{
		return FPaths::ToUtf8(FileName);
	}
	return {};
}

FString FEditorPropertyRenderer::OpenFbxFileDialog()
{
	wchar_t FileName[MAX_PATH] = L"";
	OPENFILENAMEW Ofn = {};
	Ofn.lStructSize = sizeof(Ofn);
	Ofn.hwndOwner = GetActiveWindow();
	Ofn.lpstrFile = FileName;
	Ofn.nMaxFile = MAX_PATH;
	Ofn.lpstrFilter = L"FBX Files (*.fbx)\0*.fbx\0All Files (*.*)\0*.*\0";
	Ofn.nFilterIndex = 1;
	Ofn.lpstrTitle = L"Import FBX";
	Ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (GetOpenFileNameW(&Ofn))
	{
		return FPaths::ToUtf8(FileName);
	}
	return {};
}

bool FEditorPropertyRenderer::RenderSoftObjectPropertyWidget(FPropertyValue& Prop)
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
			if (bSelectedNone)
			{
				ImGui::SetItemDefaultFocus();
			}

			const TArray<FMaterialAssetListItem>& MatFiles = FMaterialManager::Get().GetAvailableMaterialFiles();
			for (const FMaterialAssetListItem& Item : MatFiles)
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
		if (CurrentPath == "None")
		{
			Preview = "None";
		}

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
			{
				ImGui::SetItemDefaultFocus();
			}
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

		FFbxSceneImportRequest Request;
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
		if (CurrentPath == "None")
		{
			Preview = "None";
		}

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
		if (CurrentPath == "None")
		{
			Preview = "None";
		}

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
		FString Preview = (CurrentPath.empty() || CurrentPath == "None") ? "None" : GetStemFromPath(CurrentPath);

		float ButtonWidth = ImGui::CalcTextSize("Edit Script").x + ImGui::GetStyle().FramePadding.x * 2.0f;
		float Spacing = ImGui::GetStyle().ItemSpacing.x;
		ImGui::SetNextItemWidth(-(ButtonWidth + Spacing));

		if (ImGui::BeginCombo("##LuaAnimScript", Preview.c_str()))
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

			const TArray<FAssetListItem>& LuaFiles = FAssetRegistry::ListByTypeName("LuaAnimScript");
			for (const FAssetListItem& Item : LuaFiles)
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

	if (!AssetType.empty())
	{
		const TArray<FAssetListItem>& AssetFiles = FAssetRegistry::ListByTypeName(AssetType.c_str());
		if (!AssetFiles.empty() || AssetType[0] == 'U')
		{
			FString Preview = CurrentPath.empty() ? "None" : GetStemFromPath(CurrentPath);
			if (CurrentPath == "None")
			{
				Preview = "None";
			}

			if (ImGui::BeginCombo("##Asset", Preview.c_str()))
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

				for (const FAssetListItem& Item : AssetFiles)
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
	}

	FString Preview = CurrentPath.empty() ? "None" : GetStemFromPath(CurrentPath);
	if (CurrentPath == "None")
	{
		Preview = "None";
	}

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
		{
			ImGui::SetItemDefaultFocus();
		}

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

bool FEditorPropertyRenderer::RenderEnumPropertyWidget(FPropertyValue& Prop)
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
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	return bChanged;
}

bool FEditorPropertyRenderer::RenderStructPropertyWidget(FPropertyValue& Prop, FEditorPropertyRenderOptions Options)
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

		if (BeginPropertyChildTable("##StructPropertyTable"))
		{
			for (int32 ci = 0; ci < (int32)ChildProps.size(); ++ci)
			{
				ImGui::PushID(ci);

				FPropertyValue& ChildProp = ChildProps[ci];
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				const float ChildIndent = static_cast<float>(Options.IndentLevel) * StructChildIndentWidth;
				ImGui::Indent(ChildIndent);
				DrawPropertyTableLabel(GetPropertyDisplayName(ChildProp));
				ImGui::Unindent(ChildIndent);

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1);

				FEditorPropertyRenderOptions ChildOptions = Options;
				ChildOptions.IndentLevel = Options.IndentLevel + 1;
				ChildOptions.PropertyPath = MakePropertyPath(Options.PropertyPath, ChildProp.GetName());
				int32 ChildIdx = ci;
				if (RenderPropertyWidget(ChildProps, ChildIdx, ChildOptions))
				{
					bChanged = true;
				}
				ImGui::PopID();
			}
			EndPropertyChildTable();
		}
		ImGui::TreePop();
	}

	return bChanged;
}

bool FEditorPropertyRenderer::RenderArrayPropertyWidget(FPropertyValue& Prop, FEditorPropertyRenderOptions Options)
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
		if (Options.bDispatchChange)
		{
			DispatchPostEditChange(Prop, EPropertyChangeType::ArrayAdd, static_cast<int32>(Num), MakeArrayElementPath(Options.PropertyPath, static_cast<int32>(Num)));
		}
		Num = Ops->GetNum(ArrayPtr);
	}

	if (BeginPropertyChildTable("##ArrayPropertyTable"))
	{
		for (int32 ElemIdx = 0; ElemIdx < static_cast<int32>(Num); ++ElemIdx)
		{
			void* ElementPtr = Ops->GetElementPtr(ArrayPtr, static_cast<size_t>(ElemIdx));
			if (!ElementPtr)
			{
				continue;
			}

			ImGui::PushID(ElemIdx);

			FString ElementName = "Element " + std::to_string(ElemIdx);
			const FString ElementPath = MakeArrayElementPath(Options.PropertyPath, ElemIdx);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			bool bRemovedElement = false;
			if (!bEditFixedSize && Ops->RemoveAt && ImGui::Button("-"))
			{
				Ops->RemoveAt(ArrayPtr, static_cast<size_t>(ElemIdx));
				bChanged = true;
				bRemovedElement = true;
				if (Options.bDispatchChange)
				{
					DispatchPostEditChange(Prop, EPropertyChangeType::ArrayRemove, ElemIdx, ElementPath, ElementName.c_str(), ElementName.c_str());
				}
			}

			if (!bRemovedElement)
			{
				if (!bEditFixedSize && Ops->RemoveAt)
				{
					ImGui::SameLine();
				}
				DrawPropertyTableLabel(ElementName.c_str());

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1);

				FPropertyValue ElementValue;
				ElementValue.Object = Prop.Object;
				ElementValue.Property = InnerProperty;
				ElementValue.ContainerPtr = ElementPtr;

				TArray<FPropertyValue> ElementProps;
				ElementProps.push_back(ElementValue);
				int32 ElementPropIndex = 0;

				FEditorPropertyRenderOptions ElementOptions = Options;
				ElementOptions.bDispatchChange = false;
				ElementOptions.PropertyPath = ElementPath;
				if (RenderPropertyWidget(ElementProps, ElementPropIndex, ElementOptions))
				{
					bChanged = true;
					if (Options.bDispatchChange)
					{
						DispatchPostEditChange(Prop, EPropertyChangeType::ValueSet, ElemIdx, ElementPath, ElementName.c_str(), ElementName.c_str());
					}
				}
			}

			ImGui::PopID();
			if (bRemovedElement)
			{
				break;
			}
		}
		EndPropertyChildTable();
	}

	return bChanged;
}

bool FEditorPropertyRenderer::RenderPropertyWidget(TArray<FPropertyValue>& Props, int32& Index, FEditorPropertyRenderOptions Options)
{
	ImGui::PushID(Index);
	FPropertyValue& Prop = Props[Index];
	bool bChanged = false;
	Options.PropertyPath = Options.PropertyPath.empty() ? FString(Prop.GetName()) : Options.PropertyPath;
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
		{
			bChanged = ImGui::DragInt("##Value", Val, Speed, (int32)Min, (int32)Max);
		}
		else
		{
			bChanged = ImGui::DragInt("##Value", Val, Speed);
		}
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
		{
			bChanged = ImGui::DragFloat("##Value", Val, Speed, Min, Max, "%.4f");
		}
		else
		{
			bChanged = ImGui::DragFloat("##Value", Val, Speed);
		}
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
		FRotator* Rot = static_cast<FRotator*>(Prop.GetValuePtr());
		float RotXYZ[3] = { Rot->Roll, Rot->Pitch, Rot->Yaw };
		bChanged = ImGui::DragFloat3("##Value", RotXYZ, Prop.GetSpeed());
		if (bChanged)
		{
			Rot->Roll = RotXYZ[0];
			Rot->Pitch = RotXYZ[1];
			Rot->Yaw = RotXYZ[2];
			if (Options.EditedSceneComponent)
			{
				Options.EditedSceneComponent->ApplyCachedEditRotator();
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

			FFbxSceneImportRequest Request;
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
		bChanged = RenderArrayPropertyWidget(Prop, Options);
		Options.bDispatchChange = false;
		break;
	}
	case EPropertyType::Name:
	{
		FName* Val = static_cast<FName*>(Prop.GetValuePtr());
		FString Current = Val->ToString();

		TArray<FString> Names;
		FString AssetType = GetAssetTypeMetadata(Prop);
		if (AssetType.empty())
		{
			AssetType = Prop.GetName();
		}

		if (AssetType == "Font")
		{
			Names = FResourceManager::Get().GetFontNames();
		}
		else if (AssetType == "Particle")
		{
			Names = FResourceManager::Get().GetParticleNames();
		}
		else if (AssetType == "Texture")
		{
			Names = FResourceManager::Get().GetTextureNames();
		}

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
					{
						ImGui::SetItemDefaultFocus();
					}
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
		bChanged = RenderStructPropertyWidget(Prop, Options);
		Options.bDispatchChange = false;
		break;
	}
	}

	if (bReadOnly)
	{
		ImGui::EndDisabled();
		bChanged = false;
	}

	if (Options.bDispatchChange && bChanged)
	{
		DispatchPostEditChange(Prop, EPropertyChangeType::ValueSet, -1, Options.PropertyPath);
	}

	ImGui::PopID();
	return bChanged;
}
