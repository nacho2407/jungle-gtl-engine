#include "Editor/UI/EditorPropertyWidget.h"

#include "Editor/EditorEngine.h"
#include "ImGui/imgui.h"
#include "Component/StaticMeshComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/MovementComponent.h"
#include "Component/RotatingMovementComponent.h"
#include "Component/FireballComponent.h"
#include "Component/ProjectileMovementComponent.h"
#include "Core/PropertyTypes.h"
#include "Core/ResourceManager.h"
#include "Object/FName.h"
#include <functional>
#include "Component/SubUVComponent.h"
#include "Component/HeightFogComponent.h"
#include "Selection/SelectionManager.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

namespace UIConstants
{
	constexpr float XButtonSize    = 20.0f;
	constexpr float TreeRightMargin = 24.0f; // X버튼이 위치할 우측 여백
	constexpr float ClipMargin      = 28.0f; // 버튼(20) + 여백(8)
	constexpr float MinScrollHeight = 50.0f;
}

// 내부 헬퍼: X 버튼 드로잉
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

	// 컴포넌트 포인터를 ImGui PushID 용 문자열로 변환
	static void MakeXButtonId(char* OutBuf, size_t BufSize, const void* Ptr)
	{
		snprintf(OutBuf, BufSize, "xbtn_%p", Ptr);
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
		"RotatingMovement Component",
		[](AActor* Actor) -> UActorComponent* {
			URotatingMovementComponent* Comp = Actor->AddComponent<URotatingMovementComponent>();
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
};

void FEditorPropertyWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
	SelectionManager = &EditorEngine->GetSelectionManager();
}

void FEditorPropertyWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	ImGui::SetNextWindowSize(ImVec2(350.0f, 500.0f), ImGuiCond_Once);
	ImGui::Begin("Jungle Property Window");

	AActor* PrimaryActor = SelectionManager->GetPrimarySelection();
	if (!PrimaryActor)
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

	// 상단 액터 정보 및 컨트롤 영역
	RenderActorHeaderRegion(PrimaryActor, SelectedActors);

	if (SelectionManager->GetPrimarySelection() == nullptr)
	{
		ImGui::End();
		return;
	}

	// 컴포넌트 트리 영역
	SEPARATOR();
	RenderComponentTree(PrimaryActor);

	// 디테일 프로퍼티 영역
	SEPARATOR();
	ImGui::Text("Details");
	ImGui::Separator();

	float ScrollHeight = std::max(UIConstants::MinScrollHeight, ImGui::GetContentRegionAvail().y);
	ImGui::BeginChild("##Details", ImVec2(0, ScrollHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
	{
		RenderDetails(PrimaryActor, SelectedActors);
	}
	ImGui::EndChild();

	ImGui::End();
}

void FEditorPropertyWidget::UpdateSelectionState(AActor* PrimaryActor)
{
	if (PrimaryActor != LastSelectedActor)
	{
		SelectedComponent = nullptr;
		LastSelectedActor = PrimaryActor;

		USceneComponent* RootComp = PrimaryActor->GetRootComponent();
		if (RootComp && RootComp->IsA<UStaticMeshComponent>())
		{
			SelectedComponent = RootComp;
			bActorSelected = false;
		}
		else
		{
			bActorSelected = true;
		}
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

	if (bActorSelected) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
	ImGui::Text("Name: %s (+%d)", PrimaryName.c_str(), SelectionCount - 1);
	if (bActorSelected) ImGui::PopStyleColor();

	if (ImGui::IsItemClicked())
	{
		bActorSelected = true;
		SelectedComponent = nullptr;
	}

	ImGui::SameLine();
	char RemoveLabel[64];
	snprintf(RemoveLabel, sizeof(RemoveLabel), "Remove %d Objects", SelectionCount);
	if (ImGui::SmallButton(RemoveLabel))
	{
		for (AActor* Actor : SelectedActors)
		{
			if (Actor && Actor->GetWorld()) Actor->GetWorld()->DestroyActor(Actor);
		}
		SelectionManager->ClearSelection();
		SelectedComponent = nullptr;
		LastSelectedActor = nullptr;
	}
}

void FEditorPropertyWidget::RenderSingleSelectionHeader(AActor* PrimaryActor)
{
	// SelectedComponent가 아직 설정되지 않은 경우 루트로 초기화
	if (SelectedComponent == nullptr)
		SelectedComponent = PrimaryActor->GetRootComponent();

	ImGui::Text("Selected Actor: %s", PrimaryActor->GetFName().ToString().c_str());
	ImGui::Text("Selected Component: %s", SelectedComponent ? SelectedComponent->GetTypeInfo()->name : "None");

	if (bActorSelected) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
	if (ImGui::IsItemClicked())
	{
		bActorSelected = true;
		SelectedComponent = nullptr;
	}
	if (bActorSelected) ImGui::PopStyleColor();

	ImGui::SameLine();
	if (ImGui::SmallButton("Remove"))
	{
		if (PrimaryActor->GetWorld()) PrimaryActor->GetWorld()->DestroyActor(PrimaryActor);
		SelectionManager->ClearSelection();
		SelectedComponent = nullptr;
		LastSelectedActor = nullptr;
	}

	ImGui::Spacing();
	RenderAddComponentPopup(PrimaryActor);
}

void FEditorPropertyWidget::RenderAddComponentPopup(AActor* PrimaryActor)
{
	if (ImGui::Button("Add Component", ImVec2(-1, 0)))
	{
		ImGui::OpenPopup("AddComponentPopup");
	}

	if (ImGui::BeginPopup("AddComponentPopup"))
	{
		for (const FComponentMenuEntry& Entry : ComponentMenuRegistry)
		{
			if (ImGui::Selectable(Entry.DisplayName))
			{
				if (UActorComponent* NewComp = Entry.CreateAndInitFunc(PrimaryActor))
				{
					AttachAndSelectNewComponent(PrimaryActor, NewComp);
				}
			}
		}
		ImGui::EndPopup();
	}
}

void FEditorPropertyWidget::RenderComponentTree(AActor* Actor)
{
	ImGui::Text("Components");
	ImGui::Separator();

	USceneComponent* Root = Actor->GetRootComponent();
	UActorComponent* ComponentToDelete = nullptr;

	if (Root)
	{
		RenderSceneComponentNode(Actor, Root, ComponentToDelete);
	}

	// Non-scene ActorComponents (MovementComponent는 SceneComponent 아래에 표시되므로 제외)
	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (!Comp || Comp->IsA<USceneComponent>() || Comp->IsA<UMovementComponent>())
			continue;

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

		if (Comp != Actor->GetRootComponent())
		{
			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - UIConstants::TreeRightMargin);
			char XId[64];
			MakeXButtonId(XId, sizeof(XId), Comp);
			if (DrawXButton(XId)) ComponentToDelete = Comp;
		}
	}

	if (ComponentToDelete)
	{
		if (SelectedComponent == ComponentToDelete)
		{
			SelectedComponent = nullptr;
			bActorSelected = true;
		}
		Actor->RemoveComponent(ComponentToDelete);
	}
}

void FEditorPropertyWidget::RenderSceneComponentNode(AActor* Actor, USceneComponent* Comp, UActorComponent*& OutCompToDelete)
{
	if (!Comp) return;

	FString Name = Comp->GetFName().ToString();
	if (Name.empty()) Name = Comp->GetTypeInfo()->name;

	const auto& Children = Comp->GetChildren();

	// 이 SceneComponent에 연결된 MovementComponent 수집
	TArray<UMovementComponent*> AttachedMovements;
	for (UActorComponent* ActorComp : Actor->GetComponents())
	{
		UMovementComponent* MoveComp = Cast<UMovementComponent>(ActorComp);
		if (MoveComp && MoveComp->GetUpdatedComponent() == Comp)
			AttachedMovements.push_back(MoveComp);
	}

	bool bHasChildren = !Children.empty() || !AttachedMovements.empty();

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
	if (!bHasChildren) Flags |= ImGuiTreeNodeFlags_Leaf;
	if (!bActorSelected && SelectedComponent == Comp) Flags |= ImGuiTreeNodeFlags_Selected;

	bool bIsRoot = (Comp->GetParent() == nullptr);

	// 루트가 아닌 경우 X버튼 영역을 침범하지 않도록 클리핑
	if (!bIsRoot)
	{
		float ClipMaxX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x - UIConstants::ClipMargin;
		ImGui::PushClipRect(ImGui::GetWindowPos(), ImVec2(ClipMaxX, ImGui::GetWindowPos().y + 99999.f), true);
	}

	bool bOpen = ImGui::TreeNodeEx(
		Comp, Flags, "%s%s",
		bIsRoot ? "[Root] " : "",
		Name.c_str(),
		Comp->GetTypeInfo()->name
	);

	if (!bIsRoot) ImGui::PopClipRect();

	if (ImGui::IsItemClicked())
	{
		SelectedComponent = Comp;
		bActorSelected = false;
	}

	if (bOpen)
	{
		for (USceneComponent* Child : Children)
		{
			RenderSceneComponentNode(Actor, Child, OutCompToDelete);
		}

		// 연결된 MovementComponent들을 자식으로 표시
		for (UMovementComponent* MoveComp : AttachedMovements)
		{
			FString MoveName = MoveComp->GetFName().ToString();
			if (MoveName.empty()) MoveName = MoveComp->GetTypeInfo()->name;

			ImGuiTreeNodeFlags MoveFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (!bActorSelected && SelectedComponent == MoveComp) MoveFlags |= ImGuiTreeNodeFlags_Selected;

			float ClipMaxX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x - UIConstants::ClipMargin;
			ImGui::PushClipRect(ImGui::GetWindowPos(), ImVec2(ClipMaxX, ImGui::GetWindowPos().y + 99999.f), true);
			ImGui::TreeNodeEx(MoveComp, MoveFlags, "[%s] %s", MoveComp->GetTypeInfo()->name, MoveName.c_str());
			ImGui::PopClipRect();

			if (ImGui::IsItemClicked())
			{
				SelectedComponent = MoveComp;
				bActorSelected = false;
			}

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - UIConstants::TreeRightMargin);
			char XId[64];
			MakeXButtonId(XId, sizeof(XId), MoveComp);
			if (DrawXButton(XId)) OutCompToDelete = MoveComp;
		}

		ImGui::TreePop();
	}

	if (!bIsRoot)
	{
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - UIConstants::TreeRightMargin);
		char XId[64];
		MakeXButtonId(XId, sizeof(XId), Comp);
		if (DrawXButton(XId)) OutCompToDelete = Comp;
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
	ImGui::Text("Name: %s", PrimaryActor->GetFName().ToString().c_str());

	if (PrimaryActor->GetRootComponent())
	{
		ImGui::Separator();
		ImGui::Text("Transform");
		ImGui::Spacing();

		// FVector(위치, 회전, 크기)를 읽어서 Properties를 그려 주는 단순한 친구입니다.
		auto DrawTransformField = [&](const char* Label, FVector CurrentValue, auto ApplyFunc)
		{
			float Arr[3] = { CurrentValue.X, CurrentValue.Y, CurrentValue.Z };
			if (ImGui::DragFloat3(Label, Arr, 0.1f))
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

	ImGui::Separator();
	bool bVisible = PrimaryActor->IsVisible();
	if (ImGui::Checkbox("Visible", &bVisible))
	{
		PrimaryActor->SetVisible(bVisible);
	}

	ImGui::Separator();
	// Billboard 타입 체크
	if (UBillboardComponent* BillboardComp = dynamic_cast<UBillboardComponent*>(PrimaryActor->GetRootComponent()))
	{
		if (dynamic_cast<USubUVComponent*>(PrimaryActor->GetRootComponent()))
		{
			return;
		}
		ImGui::Separator();
		ImGui::Text("Sprite Texture");

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
}

void FEditorPropertyWidget::RenderComponentProperties()
{
	ImGui::Text("Component: %s", SelectedComponent->GetTypeInfo()->name);
	ImGui::Text("Name: %s", SelectedComponent->GetFName().ToString().c_str());

	ImGui::Separator();

	// PropertyDescriptor 기반 자동 위젯 렌더링
	TArray<FPropertyDescriptor> Props;
	SelectedComponent->GetEditableProperties(Props);

	AActor* Owner = SelectedComponent->GetOwner();

	for (auto& Prop : Props)
	{
		if (Prop.Type == EPropertyType::SceneComponentRef)
		{
			// SceneComponentRef는 액터 컨텍스트가 필요한 드롭다운으로 렌더링
			RenderSceneComponentRefWidget(Prop, Owner);
		}
		else
		{
			RenderPropertyWidget(Prop);
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
				*ValuePtr = SceneComp;
				SelectedComponent->PostEditProperty(Prop.Name);
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

void FEditorPropertyWidget::AttachAndSelectNewComponent(AActor* PrimaryActor, UActorComponent* NewComp)
{
	if (!PrimaryActor || !NewComp) return;

	USceneComponent* AttachTarget = nullptr;
	// 이제 SelectedComponent 멤버 변수를 정상적으로 사용할 수 있습니다!
	if (SelectedComponent && SelectedComponent->IsA<USceneComponent>())
	{
		AttachTarget = static_cast<USceneComponent*>(SelectedComponent);
	}
	else
	{
		AttachTarget = PrimaryActor->GetRootComponent();
	}

	if (USceneComponent* SceneComp = Cast<USceneComponent>(NewComp))
	{
		if (AttachTarget) SceneComp->AttachToComponent(AttachTarget);
		else PrimaryActor->SetRootComponent(SceneComp);
	}
	else if (UMovementComponent* MoveComp = Cast<UMovementComponent>(NewComp))
	{
		if (AttachTarget) MoveComp->SetUpdatedComponent(AttachTarget);
	}

	SelectedComponent = NewComp;
	bActorSelected = false;
}