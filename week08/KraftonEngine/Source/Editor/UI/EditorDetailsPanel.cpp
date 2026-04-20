#include "Editor/UI/EditorDetailsPanel.h"

#include "Editor/EditorEngine.h"

#include "ImGui/imgui.h"
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SceneComponent.h"
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
#include <filesystem>

#include "Materials/MaterialManager.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

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

static FString BuildComponentDisplayLabel(UActorComponent* Comp)
{
    if (!Comp)
    {
        return "None";
    }

    const FString TypeName = Comp->GetClass()->GetName();
    FString Name = Comp->GetFName().ToString();
    if (Name.empty())
    {
        Name = TypeName;
    }
    return TypeName + "(" + Name + ")";
}

FString FEditorDetailsPanel::OpenObjFileDialog()
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

void FEditorDetailsPanel::Render(float DeltaTime)
{
	(void)DeltaTime;

	ImGui::SetNextWindowSize(ImVec2(350.0f, 500.0f), ImGuiCond_Once);

	ImGui::Begin("Details");

	FSelectionManager& Selection = EditorEngine->GetSelectionManager();
	AActor* PrimaryActor = Selection.GetPrimarySelection();
	if (!PrimaryActor)
	{
		SelectedComponent = nullptr;
		LastSelectedActor = nullptr;
		bActorSelected = true;
		ImGui::TextDisabled("No actor selected.");
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

	// ========== 고정 영역: Actor Info ==========
	ImGui::Text("Class: %s", PrimaryActor->GetClass()->GetName());

	FString PrimaryName = PrimaryActor->GetFName().ToString();
	if (PrimaryName.empty()) PrimaryName = PrimaryActor->GetClass()->GetName();

	if (bActorSelected) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
	if (SelectionCount > 1)
	{
		ImGui::Text("Name: %s (+%d)", PrimaryName.c_str(), SelectionCount - 1);
	}
	else
	{
		ImGui::Text("Name: %s", PrimaryName.c_str());
	}
	if (bActorSelected) ImGui::PopStyleColor();
	if (ImGui::IsItemClicked())
	{
		bActorSelected = true;
		SelectedComponent = nullptr;
	}

	// ========== 고정 영역: Component Tree ==========
	SEPARATOR();
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

void FEditorDetailsPanel::RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
{
	if (bActorSelected)
	{
		RenderActorProperties(PrimaryActor, SelectedActors);
	}
	else if (SelectedComponent)
	{
		RenderComponentProperties(PrimaryActor);
	}
	else
	{
		ImGui::TextDisabled("Select an actor or component to view details.");
	}
}

void FEditorDetailsPanel::RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors)
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

void FEditorDetailsPanel::RenderComponentTree(AActor* Actor)
{
	ImGui::Text("Component List");

	// Get All Component Classes
	TArray<UClass*>& AllClasses = UClass::GetAllClasses();

	TArray<UClass*> ComponentClasses;
	for (UClass* Cls : AllClasses)
	{
		if (Cls->IsA(UActorComponent::StaticClass()) && !Cls->HasAnyClassFlags(CF_Abstract))
			ComponentClasses.push_back(Cls);
	}

	static int SelectedIndex = 0;
	if (ComponentClasses.empty())
	{
		SelectedIndex = 0;
	}
	else if (SelectedIndex >= static_cast<int>(ComponentClasses.size()))
	{
		SelectedIndex = static_cast<int>(ComponentClasses.size()) - 1;
	}
	const char* Preview = ComponentClasses.empty() ? "None" : ComponentClasses[SelectedIndex]->GetName();

	USceneComponent* Root = Actor->GetRootComponent();

	if (!ComponentClasses.empty() && ImGui::Button("Add"))
	{
		UActorComponent* Comp = Actor->AddComponentByClass(ComponentClasses[SelectedIndex]);
		if (!Comp)
		{
			return;
		}

		if (ComponentClasses[SelectedIndex]->IsA(USceneComponent::StaticClass()))
		{
			if (SelectedComponent != nullptr && SelectedComponent->GetClass()->IsA(USceneComponent::StaticClass()))
				Cast<USceneComponent>(Comp)->AttachToComponent(Cast<USceneComponent>(SelectedComponent));
			else
				Cast<USceneComponent>(Comp)->AttachToComponent(Root);
		}
	}

	ImGui::SameLine();
	// ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::BeginCombo("Type", Preview))
	{
		for (int i = 0; i < (int)ComponentClasses.size(); ++i)
		{
			bool bSelected = (SelectedIndex == i);
			if (ImGui::Selectable(ComponentClasses[i]->GetName(), bSelected))
				SelectedIndex = i;
			if (bSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
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

		FString Label = BuildComponentDisplayLabel(Comp);

		ImGui::Indent(12.0f);
		ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (!bActorSelected && SelectedComponent == Comp)
			Flags |= ImGuiTreeNodeFlags_Selected;

		ImGui::TreeNodeEx(Comp, Flags, "%s", Label.c_str());
		if (ImGui::IsItemClicked())
		{
			SelectedComponent = Comp;
			bActorSelected = false;
		}
		ImGui::Unindent(12.0f);
	}
}

void FEditorDetailsPanel::RenderSceneComponentNode(USceneComponent* Comp)
{
	if (!Comp) return;

	FString Label = BuildComponentDisplayLabel(Comp);

	const auto& Children = Comp->GetChildren();
	bool bHasChildren = !Children.empty();

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
	if (!bHasChildren)
		Flags |= ImGuiTreeNodeFlags_Leaf;
	if (!bActorSelected && SelectedComponent == Comp)
		Flags |= ImGuiTreeNodeFlags_Selected;

	bool bIsRoot = (Comp->GetParent() == nullptr);
	bool bOpen = ImGui::TreeNodeEx(
		Comp, Flags, "%s%s",
		bIsRoot ? "[Root] " : "",
		Label.c_str()
	);

	if (ImGui::IsItemClicked())
	{
		SelectedComponent = Comp;
		bActorSelected = false;
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

void FEditorDetailsPanel::RenderComponentProperties(AActor* Actor)
{
	(void)Actor;
	const FString ComponentLabel = BuildComponentDisplayLabel(SelectedComponent);
	ImGui::Text("Component: %s", ComponentLabel.c_str());
	ImGui::Separator();

	TArray<FPropertyDescriptor> Props;
	SelectedComponent->GetEditableProperties(Props);

	auto IsTransformProp = [](const FString& Name) {
		return Name == "Location" || Name == "Rotation" || Name == "Scale";
	};
	auto IsStaticMeshProp = [](const FString& Name, EPropertyType Type) {
		return Type == EPropertyType::StaticMeshRef || Name == "Static Mesh" || Name == "StaticMesh";
	};
	auto IsMaterialProp = [](const FString& Name, EPropertyType Type) {
		return Type == EPropertyType::MaterialSlot || Name.rfind("Element ", 0) == 0;
	};

	bool bIsRoot = false;
	if (SelectedComponent->IsA<USceneComponent>())
	{
		USceneComponent* SceneComp = static_cast<USceneComponent*>(SelectedComponent);
		bIsRoot = (SceneComp->GetParent() == nullptr);
	}

	if (!bIsRoot && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (int32 i = 0; i < (int32)Props.size(); ++i)
		{
			if (IsTransformProp(Props[i].Name))
			{
				RenderDetailsPanel(Props, i);
			}
		}
	}

	const bool bIsStaticMeshComponent = SelectedComponent->IsA<UStaticMeshComponent>();
	if (bIsStaticMeshComponent && ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (int32 i = 0; i < (int32)Props.size(); ++i)
		{
			if (IsStaticMeshProp(Props[i].Name, Props[i].Type))
			{
				bool bChanged = RenderDetailsPanel(Props, i);
				if (bChanged && Props[i].Type == EPropertyType::StaticMeshRef)
				{
					if (SelectedComponent->IsA<USceneComponent>())
					{
						static_cast<USceneComponent*>(SelectedComponent)->MarkTransformDirty();
					}
					return;
				}
			}
		}
	}

	if (bIsStaticMeshComponent && ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (int32 i = 0; i < (int32)Props.size(); ++i)
		{
			if (IsMaterialProp(Props[i].Name, Props[i].Type))
			{
				RenderDetailsPanel(Props, i);
			}
		}
	}

	if (ImGui::CollapsingHeader("Details", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (int32 i = 0; i < (int32)Props.size(); ++i)
		{
			if (IsTransformProp(Props[i].Name) || IsStaticMeshProp(Props[i].Name, Props[i].Type) || IsMaterialProp(Props[i].Name, Props[i].Type))
			{
				continue;
			}
			RenderDetailsPanel(Props, i);
		}
	}

	if (SelectedComponent->IsA<USceneComponent>())
	{
		static_cast<USceneComponent*>(SelectedComponent)->MarkTransformDirty();
	}
}

bool FEditorDetailsPanel::RenderDetailsPanel(TArray<FPropertyDescriptor>& Props, int32& Index)
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
		bChanged = ImGui::DragInt(Prop.Name.c_str(), Val);
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
	case EPropertyType::StaticMeshRef:
	{
		FString* Val = static_cast<FString*>(Prop.ValuePtr);

		FString Preview = Val->empty() ? "None" : GetStemFromPath(*Val);
		if (*Val == "None") Preview = "None";

		ImGui::Text("%s", Prop.Name.c_str());
		ImGui::SameLine(120);
		ImGui::SetNextItemWidth(-1.0f);

		float ButtonWidth = ImGui::CalcTextSize("Import OBJ").x + ImGui::GetStyle().FramePadding.x * 2.0f;

		if (ImGui::BeginCombo("##Mesh", Preview.c_str()))
		{
			FObjManager::ScanMeshAssets();
			FObjManager::ScanObjSourceFiles();

			bool bSelectedNone = (*Val == "None");
			if (ImGui::Selectable("None", bSelectedNone))
			{
				*Val = "None";
				bChanged = true;
			}
			if (bSelectedNone)
				ImGui::SetItemDefaultFocus();

			ImGui::TextDisabled("OBJ Source");
			const TArray<FMeshAssetListItem>& ObjFiles = FObjManager::GetAvailableObjFiles();
			for (const FMeshAssetListItem& Item : ObjFiles)
			{
				const FString Label = Item.DisplayName + "##obj_" + Item.FullPath;
				bool bSelected = (*Val == Item.FullPath);
				if (ImGui::Selectable(Label.c_str(), bSelected))
				{
					*Val = Item.FullPath;
					bChanged = true;
				}
				if (bSelected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::Separator();
			ImGui::TextDisabled("Cached Mesh");
			const TArray<FMeshAssetListItem>& MeshFiles = FObjManager::GetAvailableMeshFiles();
			for (const FMeshAssetListItem& Item : MeshFiles)
			{
				const FString Label = Item.DisplayName + "##bin_" + Item.FullPath;
				bool bSelected = (*Val == Item.FullPath);
				if (ImGui::Selectable(Label.c_str(), bSelected))
				{
					*Val = Item.FullPath;
					bChanged = true;
				}
				if (bSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		// .obj 임포트 버튼은 다음 줄 우측 정렬
		ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y));
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
					*Val = ObjPath;
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

		FString Preview = (Slot->Path.empty() || Slot->Path == "None") ? "None" : GetStemFromPath(Slot->Path);
		if (ImGui::BeginCombo("##Mat", Preview.c_str()))
		{
			FMaterialManager::Get().ScanMaterialAssets();

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
