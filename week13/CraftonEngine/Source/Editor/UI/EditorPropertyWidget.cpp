#include "Editor/UI/EditorPropertyWidget.h"

#include "Editor/EditorEngine.h"

#include "ImGui/imgui.h"
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SceneComponent.h"
#include "Core/PropertyTypes.h"
#include "Core/ResourceManager.h"
#include "Object/FName.h"
#include "Object/ObjectIterator.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

void FEditorPropertyWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	ImGui::SetNextWindowSize(ImVec2(350.0f, 500.0f), ImGuiCond_Once);

	ImGui::Begin("Jungle Property Window");

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
		ImGui::Text("Class: %s", PrimaryActor->GetTypeInfo()->name);

		FString PrimaryName = PrimaryActor->GetFName().ToString();
		if (PrimaryName.empty()) PrimaryName = PrimaryActor->GetTypeInfo()->name;

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
			for (AActor* Actor : SelectedActors)
			{
				if (Actor && Actor->GetWorld())
				{
					Actor->GetWorld()->DestroyActor(Actor);
				}
			}
			Selection.ClearSelection();
			SelectedComponent = nullptr;
			LastSelectedActor = nullptr;
			ImGui::End();
			return;
		}
	}
	else
	{
		ImGui::Text("Class: %s", PrimaryActor->GetTypeInfo()->name);

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
			if (PrimaryActor->GetWorld())
			{
				PrimaryActor->GetWorld()->DestroyActor(PrimaryActor);
			}
			Selection.ClearSelection();
			SelectedComponent = nullptr;
			LastSelectedActor = nullptr;
			ImGui::End();
			return;
		}
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
	ImGui::Text("Name: %s", PrimaryActor->GetFName().ToString().c_str());

	if (PrimaryActor->GetRootComponent())
	{
		ImGui::Separator();
		ImGui::Text("Transform");
		ImGui::Spacing();

		FVector Pos = PrimaryActor->GetActorLocation();
		float PosArray[3] = { Pos.X, Pos.Y, Pos.Z };

		FVector Rot = PrimaryActor->GetActorRotation();
		float RotArray[3] = { Rot.X, Rot.Y, Rot.Z };

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
		if (ImGui::DragFloat3("Rotation", RotArray, 0.1f))
		{
			FVector Delta = FVector(RotArray[0], RotArray[1], RotArray[2]) - Rot;
			for (AActor* Actor : SelectedActors)
			{
				if (Actor) Actor->SetActorRotation(Actor->GetActorRotation() + Delta);
			}
			EditorEngine->GetGizmo()->UpdateGizmoTransform();
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

	UStaticMeshComp* TargetComp = nullptr;
	TargetComp = Cast<UStaticMeshComp>(PrimaryActor->GetRootComponent());
	if (TargetComp)
	{
		// 나중에 바뀜
		FString currentStaticMeshName = "None";
		if (TargetComp->GetStaticMesh())
		{
			currentStaticMeshName = TargetComp->GetStaticMesh()->GetAssetPathFileName();
		}

		ImGui::Separator();
		if (ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// 1. 드롭다운을 여는 버튼
			if (ImGui::Button(currentStaticMeshName.c_str())) {
				ImGui::OpenPopup("StaticMeshDropdown");
			}

			ImVec2 buttonBottomLeft = ImGui::GetItemRectMin();
			ImGui::SetNextWindowPos(buttonBottomLeft, ImGuiCond_Always, ImVec2(0.0f, 1.0f));

			// 2. 팝업 내용 구성
			if (ImGui::BeginPopup("StaticMeshDropdown")) {
				// --- 검색창 ---
				//ImGui::TextDisabled("탐색");
				//static char searchBuffer[128] = "";
				//ImGui::InputText("##Search", searchBuffer, IM_ARRAYSIZE(searchBuffer));

				// --- 에셋 리스트 (스크롤 영역) ---
				// 높이를 지정하여 스크롤바가 생기도록 함
				if (ImGui::BeginChild("StaticMeshList", ImVec2(0, 200), true))
				{
					for (TObjectIterator<UStaticMesh> It; It; ++It)
					{
						UStaticMesh* MeshAsset = *It;
						if (!MeshAsset) continue;

						FString AssetName = MeshAsset->GetAssetPathFileName().c_str();

						bool bIsSelected = (currentStaticMeshName == MeshAsset->GetAssetPathFileName());

						if (ImGui::Selectable(AssetName.c_str(), bIsSelected))
						{
							TargetComp->SetStaticMesh(MeshAsset);

							ImGui::CloseCurrentPopup();
						}

						if (bIsSelected)
						{
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndChild();
				}
				ImGui::EndPopup();
			}
		}
	}

	ImGui::Separator();
	if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// 1. 드롭다운을 여는 버튼
		if (ImGui::Button("MI_Cube ▼")) {
			ImGui::OpenPopup("MaterialDropdown");
		}

		ImVec2 buttonBottomLeft = ImGui::GetItemRectMin();
		ImGui::SetNextWindowPos(buttonBottomLeft, ImGuiCond_Always, ImVec2(0.0f, 1.0f));

		// 2. 팝업 내용 구성
		if (ImGui::BeginPopup("MaterialDropdown")) {
			// --- 검색창 ---
			ImGui::TextDisabled("탐색");
			static char searchBuffer[128] = "";
			ImGui::InputText("##Search", searchBuffer, IM_ARRAYSIZE(searchBuffer));

			// --- 에셋 리스트 (스크롤 영역) ---
			// 높이를 지정하여 스크롤바가 생기도록 함
			if (ImGui::BeginChild("MaterialList", ImVec2(0, 200), true)) {

				//for (const auto& AssetName : AllStaticMeshes) {
				//	// 검색 필터링 로직
				//	if (strstr(AssetName.c_str(), searchBuffer) == nullptr) continue;

				//	// 아이콘과 텍스트 배치
				//	// ImGui::Image((void*)TextureID, ImVec2(16, 16)); // 아이콘이 있다면 추가
				//	// ImGui::SameLine();
				//	if (ImGui::Selectable(AssetName.c_str())) {
				//		// 에셋 선택 시 처리 로직
				//		SelectedAsset = AssetName;
				//		ImGui::CloseCurrentPopup(); // 선택 후 팝업 닫기
				//	}
				//}
				ImGui::EndChild();
			}
			ImGui::EndPopup();
		}
	}
}

void FEditorPropertyWidget::RenderComponentTree(AActor* Actor)
{
	ImGui::Text("Components");
	ImGui::Separator();

	USceneComponent* Root = Actor->GetRootComponent();

	if (Root)
	{
		RenderSceneComponentNode(Root);
	}

	// Non-scene ActorComponents
	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (!Comp) continue;
		if (Comp->IsA<USceneComponent>()) continue;

		FString Name = Comp->GetFName().ToString();
		if (Name.empty()) Name = Comp->GetTypeInfo()->name;

		ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (!bActorSelected && SelectedComponent == Comp)
			Flags |= ImGuiTreeNodeFlags_Selected;

		ImGui::TreeNodeEx(Comp, Flags, "[%s] %s", Comp->GetTypeInfo()->name, Name.c_str());
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

	FString Name = Comp->GetFName().ToString();
	if (Name.empty()) Name = Comp->GetTypeInfo()->name;

	const auto& Children = Comp->GetChildren();
	bool bHasChildren = !Children.empty();

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
	if (!bHasChildren)
		Flags |= ImGuiTreeNodeFlags_Leaf;
	if (!bActorSelected && SelectedComponent == Comp)
		Flags |= ImGuiTreeNodeFlags_Selected;

	bool bIsRoot = (Comp->GetParent() == nullptr);
	bool bOpen = ImGui::TreeNodeEx(
		Comp, Flags, "%s%s (%s)",
		bIsRoot ? "[Root] " : "",
		Name.c_str(),
		Comp->GetTypeInfo()->name
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

void FEditorPropertyWidget::RenderComponentProperties()
{
	ImGui::Text("Component: %s", SelectedComponent->GetTypeInfo()->name);
	ImGui::Text("Name: %s", SelectedComponent->GetFName().ToString().c_str());
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
	auto IsTransformProp = [](const char* Name) {
		return strcmp(Name, "Location") == 0
			|| strcmp(Name, "Rotation") == 0
			|| strcmp(Name, "Scale") == 0;
		};

	// Pass 1: Transform 프로퍼티 먼저 (Root가 아닐 때만)
	if (!bIsRoot)
	{
		for (auto& Prop : Props)
		{
			if (IsTransformProp(Prop.Name))
				RenderPropertyWidget(Prop);
		}
		ImGui::Separator();
	}

	// Pass 2: 나머지 프로퍼티
	for (auto& Prop : Props)
	{
		if (IsTransformProp(Prop.Name))
			continue;
		RenderPropertyWidget(Prop);
	}

	// 프로퍼티 직접 편집 후 월드 행렬 갱신
	if (SelectedComponent->IsA<USceneComponent>())
	{
		static_cast<USceneComponent*>(SelectedComponent)->MarkTransformDirty();
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
	case EPropertyType::String:
	{
		FString* Val = static_cast<FString*>(Prop.ValuePtr);
		char Buf[256];
		strncpy_s(Buf, sizeof(Buf), Val->c_str(), _TRUNCATE);
		if (ImGui::InputText(Prop.Name, Buf, sizeof(Buf)))
		{
			*Val = Buf;
			bChanged = true;
		}
		break;
	}
	case EPropertyType::Name:
	{
		FName* Val = static_cast<FName*>(Prop.ValuePtr);
		FString Current = Val->ToString();

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
	}

	if (bChanged && SelectedComponent)
	{
		SelectedComponent->PostEditProperty(Prop.Name);
	}
}