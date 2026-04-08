#include "PropertyWindow.h"
#include "EditorEngine.h"
#include "Actor/Actor.h"
#include "Component/ActorComponent.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/RandomColorComponent.h"
#include "Component/SceneComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextComponent.h"
#include "Component/UUIDBillboardComponent.h"
#include "Component/BillboardComponent.h"
#include "Level/Level.h"
#include "Object/Class.h"
#include "Object/ObjectFactory.h"
#include "Object/ObjectIterator.h"
#include "Renderer/MeshData.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"

namespace
{
	using FComponentClassGetter = UClass * (*)();

	struct FComponentAddOption
	{
		const char* Label;
		const char* BaseName;
		FComponentClassGetter GetClass;
	};

	const FComponentAddOption GComponentAddOptions[] =
	{
		{ "Scene Component", "SceneComponent", &USceneComponent::StaticClass },
		{ "Static Mesh Component", "StaticMeshComponent", &UStaticMeshComponent::StaticClass },
		{ "Text Component", "TextComponent", &UTextRenderComponent::StaticClass },
		{ "SubUV Component", "SubUVComponent", &USubUVComponent::StaticClass },
	};

	FString BuildUniqueComponentName(AActor* SelectedActor, const FString& BaseName)
	{
		if (!SelectedActor)
		{
			return BaseName;
		}

		auto HasSameName = [SelectedActor](const FString& CandidateName)
		{
			for (UActorComponent* Component : SelectedActor->GetComponents())
			{
				if (Component && Component->GetName() == CandidateName)
				{
					return true;
				}
			}
			return false;
		};

		FString UniqueName = BaseName;
		int32 Suffix = 1;
		while (HasSameName(UniqueName))
		{
			UniqueName = BaseName + std::to_string(Suffix++);
		}

		return UniqueName;
	}

	void RefreshSceneComponentHierarchy(USceneComponent* Component)
	{
		if (!Component)
		{
			return;
		}

		if (Component->IsA(UPrimitiveComponent::StaticClass()))
		{
			static_cast<UPrimitiveComponent*>(Component)->UpdateBounds();
		}

		for (USceneComponent* Child : Component->GetAttachChildren())
		{
			RefreshSceneComponentHierarchy(Child);
		}
	}
}

bool FPropertyWindow::IsComponentOwnedByActor(AActor* SelectedActor, UActorComponent* Component) const
{
	if (!SelectedActor || !Component)
	{
		return false;
	}

	for (UActorComponent* OwnedComponent : SelectedActor->GetComponents())
	{
		if (OwnedComponent == Component)
		{
			return true;
		}
	}

	return false;
}

USceneComponent* FPropertyWindow::GetSelectedSceneComponent(AActor* SelectedActor) const
{
	if (!IsComponentOwnedByActor(SelectedActor, SelectedComponent))
	{
		return nullptr;
	}

	if (!SelectedComponent->IsA(USceneComponent::StaticClass()))
	{
		return nullptr;
	}

	return static_cast<USceneComponent*>(SelectedComponent);
}

void FPropertyWindow::SetTarget(const FVector& Location, const FVector& Rotation,
                                const FVector& Scale, const char* ActorName)
{
	EditLocation = Location;
	EditRotation = Rotation;
	EditScale = Scale;
	bModified = false;

	if (ActorName)
		snprintf(ActorNameBuf, sizeof(ActorNameBuf), "%s", ActorName);
	else
		snprintf(ActorNameBuf, sizeof(ActorNameBuf), "None");
}

void FPropertyWindow::DrawTransformSection()
{
}

bool FPropertyWindow::DrawVector3Control(const char* Label, const FVector& Value, FVector& OutValue, float Speed, const char* Format)
{
	float Values[3] = { Value.X, Value.Y, Value.Z };
	ImGui::PushItemWidth(-1.0f);
	const bool bChanged = ImGui::DragFloat3(Label, Values, Speed, 0.0f, 0.0f, Format);
	ImGui::PopItemWidth();
	if (bChanged)
	{
		OutValue = FVector(Values[0], Values[1], Values[2]);
	}
	return bChanged;
}

void FPropertyWindow::DrawSceneComponentDetails(USceneComponent* SceneComponent)
{
	if (!SceneComponent)
	{
		return;
	}

	ImGui::TextDisabled("Transform");

	FTransform RelativeTransform = SceneComponent->GetRelativeTransform();
	bool bChangedTransform = false;

	FVector NewLocation = RelativeTransform.GetTranslation();
	if (DrawVector3Control("Location", RelativeTransform.GetTranslation(), NewLocation, 0.1f, "%.2f"))
	{
		RelativeTransform.SetTranslation(NewLocation);
		bChangedTransform = true;
	}

	const FVector CurrentEuler = RelativeTransform.Rotator().Euler();
	FVector NewEuler = CurrentEuler;
	if (DrawVector3Control("Rotation", CurrentEuler, NewEuler, 0.5f, "%.1f"))
	{
		RelativeTransform.SetRotation(FRotator::MakeFromEuler(NewEuler));
		bChangedTransform = true;
	}

	FVector NewScale = RelativeTransform.GetScale3D();
	if (DrawVector3Control("Scale", RelativeTransform.GetScale3D(), NewScale, 0.01f, "%.3f"))
	{
		RelativeTransform.SetScale3D(NewScale);
		bChangedTransform = true;
	}

	if (bChangedTransform)
	{
		SceneComponent->SetRelativeTransform(RelativeTransform);
		RefreshSceneComponentHierarchy(SceneComponent);
		if (AActor* Owner = SceneComponent->GetOwner())
		{
			if (ULevel* Level = Owner->GetLevel())
			{
				Level->MarkSpatialDirty();
			}
		}
	}
}

void FPropertyWindow::DrawStaticMeshComponentDetails(UStaticMeshComponent* MeshComponent)
{
	if (!MeshComponent)
	{
		return;
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Static Mesh");

	UStaticMesh* CurrentMesh = MeshComponent->GetStaticMesh();
	const std::string CurrentMeshName = CurrentMesh ? CurrentMesh->GetAssetPathFileName() : "None";

	ImGui::PushItemWidth(-1.0f);
	if (ImGui::BeginCombo("Mesh Asset", CurrentMeshName.c_str()))
	{
		for (TObjectIterator<UStaticMesh> It; It; ++It)
		{
			UStaticMesh* MeshAsset = It.Get();
			if (!MeshAsset)
			{
				continue;
			}

			const std::string MeshName = MeshAsset->GetAssetPathFileName();
			const bool bSelected = (CurrentMesh == MeshAsset);
			if (ImGui::Selectable(MeshName.c_str(), bSelected))
			{
				MeshComponent->SetStaticMesh(MeshAsset);
				MeshComponent->UpdateBounds();
				if (AActor* Owner = MeshComponent->GetOwner())
				{
					if (ULevel* Level = Owner->GetLevel())
					{
						Level->MarkSpatialDirty();
					}
				}
			}

			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();

	if (!CurrentMesh)
	{
		ImGui::TextDisabled("No Static Mesh Assigned");
		return;
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Materials");

	TArray<FString> MaterialNames = FMaterialManager::Get().GetAllMaterialNames();
	const uint32 NumSections = CurrentMesh->GetNumSections();

	if (ImGui::BeginCombo("Apply To All", "Select Material..."))
	{
		for (const FString& MaterialName : MaterialNames)
		{
			if (ImGui::Selectable(MaterialName.c_str(), false))
			{
				if (std::shared_ptr<FMaterial> Material = FMaterialManager::Get().FindByName(MaterialName))
				{
					for (uint32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
					{
						MeshComponent->SetMaterial(SectionIndex, Material);
					}
				}
			}
		}
		ImGui::EndCombo();
	}

	float MasterScroll[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	if (NumSections > 0)
	{
		if (std::shared_ptr<FMaterial> FirstMaterial = MeshComponent->GetMaterial(0))
		{
			FirstMaterial->GetParameterData("UVScrollSpeed", MasterScroll, sizeof(MasterScroll));
		}
	}

	if (ImGui::DragFloat2("Scroll All Sections", MasterScroll, 0.001f, -5.0f, 5.0f, "%.2f"))
	{
		for (uint32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
		{
			if (std::shared_ptr<FMaterial> Material = MeshComponent->GetMaterial(SectionIndex))
			{
				Material->SetParameterData("UVScrollSpeed", MasterScroll, sizeof(MasterScroll));
			}
		}
	}

	for (uint32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		std::shared_ptr<FMaterial> CurrentMaterial = MeshComponent->GetMaterial(SectionIndex);
		const std::string CurrentMaterialName = CurrentMaterial ? CurrentMaterial->GetOriginName() : "None";
		const std::string ComboLabel = "Section " + std::to_string(SectionIndex);

		ImGui::PushID(static_cast<int>(SectionIndex));
		ImGui::PushItemWidth(-1.0f);
		if (ImGui::BeginCombo(ComboLabel.c_str(), CurrentMaterialName.c_str()))
		{
			for (const FString& MaterialName : MaterialNames)
			{
				const bool bSelected = (CurrentMaterialName == MaterialName);
				if (ImGui::Selectable(MaterialName.c_str(), bSelected))
				{
					if (std::shared_ptr<FMaterial> Material = FMaterialManager::Get().FindByName(MaterialName))
					{
						MeshComponent->SetMaterial(SectionIndex, Material);
						CurrentMaterial = Material;
					}
				}
				if (bSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();

		if (CurrentMaterial)
		{
			FVector4 BaseColor = CurrentMaterial->GetVectorParameter("BaseColor");
			float ColorArray[4] = { BaseColor.X, BaseColor.Y, BaseColor.Z, BaseColor.W };
			if (ImGui::ColorEdit4("Base Color", ColorArray))
			{
				CurrentMaterial->SetParameterData("BaseColor", ColorArray, sizeof(ColorArray));
			}

			float ScrollArray[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			CurrentMaterial->GetParameterData("UVScrollSpeed", ScrollArray, sizeof(ScrollArray));
			if (ImGui::DragFloat2("UV Scroll", ScrollArray, 0.001f, -5.0f, 5.0f, "%.2f"))
			{
				CurrentMaterial->SetParameterData("UVScrollSpeed", ScrollArray, sizeof(ScrollArray));
			}
		}
		ImGui::Spacing();
		ImGui::PopID();
	}
}

void FPropertyWindow::DrawTextComponentDetails(UTextRenderComponent* TextComponent)
{
	if (!TextComponent)
	{
		return;
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Text");

	char TextBuffer[256] = {};
	snprintf(TextBuffer, sizeof(TextBuffer), "%s", TextComponent->GetText().c_str());
	if (ImGui::InputText("Text", TextBuffer, sizeof(TextBuffer)))
	{
		TextComponent->SetText(TextBuffer);
		TextComponent->UpdateBounds();
		if (AActor* Owner = TextComponent->GetOwner())
		{
			if (ULevel* Level = Owner->GetLevel())
			{
				Level->MarkSpatialDirty();
			}
		}
	}

	FVector4 TextColor = TextComponent->GetTextColor();
	float ColorArray[4] = { TextColor.X, TextColor.Y, TextColor.Z, TextColor.W };
	if (ImGui::ColorEdit4("Text Color", ColorArray))
	{
		TextComponent->SetTextColor(FVector4(ColorArray[0], ColorArray[1], ColorArray[2], ColorArray[3]));
		TextComponent->MarkTextMeshDirty();
	}

	float TextScale = TextComponent->GetTextScale();
	if (ImGui::DragFloat("Text Scale", &TextScale, 0.01f, 0.01f, 100.0f, "%.2f"))
	{
		TextComponent->SetTextScale(TextScale);
		TextComponent->UpdateBounds();
		if (AActor* Owner = TextComponent->GetOwner())
		{
			if (ULevel* Level = Owner->GetLevel())
			{
				Level->MarkSpatialDirty();
			}
		}
	}

	bool bBillboard = TextComponent->IsBillboard();
	if (ImGui::Checkbox("Billboard", &bBillboard))
	{
		TextComponent->SetBillboard(bBillboard);
	}


	const char* HAlignOptions[] = { "Left", "Center", "Right" };
	int HAlignIndex = (int)TextComponent->GetHorizontalAlignment();
	if (ImGui::Combo("Horizontal Alignment", &HAlignIndex, HAlignOptions, IM_ARRAYSIZE(HAlignOptions)))
		TextComponent->SetHorizontalAlignment((EHorizTextAligment)HAlignIndex);

	const char* VAlignOptions[] = { "Top", "Center", "Bottom" };
	int VAlignIndex = (int)TextComponent->GetVerticalAlignment();
	if (ImGui::Combo("Vertical Alignment", &VAlignIndex, VAlignOptions, IM_ARRAYSIZE(VAlignOptions)))
		TextComponent->SetVerticalAlignment((EVerticalTextAligment)VAlignIndex);
}

void FPropertyWindow::DrawSubUVComponentDetails(USubUVComponent* SubUVComponent)
{
	if (!SubUVComponent)
	{
		return;
	}

	ImGui::Spacing();
	ImGui::TextDisabled("SubUV");

	FVector2 Size = SubUVComponent->GetSize();
	float SizeArray[2] = { Size.X, Size.Y };
	if (ImGui::DragFloat2("Size", SizeArray, 0.01f, 0.01f, 100.0f, "%.2f"))
	{
		SubUVComponent->SetSize(FVector2(SizeArray[0], SizeArray[1]));
		SubUVComponent->UpdateBounds();
		if (AActor* Owner = SubUVComponent->GetOwner())
		{
			if (ULevel* Level = Owner->GetLevel())
			{
				Level->MarkSpatialDirty();
			}
		}
	}

	float FPS = SubUVComponent->GetFPS();
	if (ImGui::DragFloat("FPS", &FPS, 0.1f, 0.0f, 240.0f, "%.1f"))
	{
		SubUVComponent->SetFPS(FPS);
	}

	bool bLoop = SubUVComponent->IsLoop();
	if (ImGui::Checkbox("Loop", &bLoop))
	{
		SubUVComponent->SetLoop(bLoop);
	}

	bool bBillboard = SubUVComponent->IsBillboard();
	if (ImGui::Checkbox("Billboard", &bBillboard))
	{
		SubUVComponent->SetBillboard(bBillboard);
	}
}

void FPropertyWindow::DrawBillboardComponentDetials(UBillboardComponent* BillboardComponent, FEditorEngine* Engine)
{
	std::wstring CurrentPath = BillboardComponent->GetTexturePath();
	std::string CurrentFileName = std::filesystem::path(CurrentPath).filename().string();
	if (ImGui::BeginCombo("Sprite", CurrentFileName.c_str()))
	{
		for (auto& Pair : Engine->GetRenderer()->GetBillboardRenderer().GetTextureCache())
		{
			std::string FileName = std::filesystem::path(Pair.first).filename().string();
			bool bSelected = (Pair.first == CurrentPath);
			if (ImGui::Selectable(FileName.c_str(), bSelected))
				BillboardComponent->SetTexturePath(Pair.first); 
			if (bSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::Separator();

	float Size[2] = { BillboardComponent->GetSize().X, BillboardComponent->GetSize().Y };
	if (ImGui::DragFloat2("Size", Size, 0.01f, 0.01f, 100.f, "%.2f"))
		BillboardComponent->SetSize(FVector2(Size[0], Size[1]));

	float U = BillboardComponent->GetUVMin().X;
	float V = BillboardComponent->GetUVMin().Y;
	float UL = BillboardComponent->GetUVMax().X;
	float VL = BillboardComponent->GetUVMax().Y;

	ImGui::Separator();

	ImGui::DragFloat("U", &U, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("V", &V, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("UL", &UL, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("VL", &VL, 0.01f, 0.0f, 1.0f);

	BillboardComponent->SetUVMin(FVector2(U, V));
	BillboardComponent->SetUVMax(FVector2(UL, VL));
}

void FPropertyWindow::DrawDetailsSection(UActorComponent* Component, FEditorEngine* Engine)
{
	if (!Component)
	{
		ImGui::TextDisabled("Select a component from the Components panel.");
		return;
	}

	const FString ClassName = Component->GetClass() ? Component->GetClass()->GetName() : "UActorComponent";
	const FString ComponentName = Component->GetName().empty() ? ClassName : Component->GetName();

	ImGui::Text("Name: %s", ComponentName.c_str());
	ImGui::Text("Class: %s", ClassName.c_str());
	ImGui::Text("Registered: %s", Component->IsRegistered() ? "Yes" : "No");

	bool bTickEnabled = Component->IsComponentTickEnabled();
	if (ImGui::Checkbox("Tick Enabled", &bTickEnabled))
	{
		Component->SetComponentTickEnabled(bTickEnabled);
	}

	if (Component->IsA(USceneComponent::StaticClass()))
	{
		DrawSceneComponentDetails(static_cast<USceneComponent*>(Component));
	}

	if (Component->IsA(UStaticMeshComponent::StaticClass()))
	{
		DrawStaticMeshComponentDetails(static_cast<UStaticMeshComponent*>(Component));
	}

	if (Component->IsA(UTextRenderComponent::StaticClass()) && !Component->IsA(UUUIDBillboardComponent::StaticClass()))
	{
		DrawTextComponentDetails(static_cast<UTextRenderComponent*>(Component));
	}

	if (Component->IsA(USubUVComponent::StaticClass()))
	{
		DrawSubUVComponentDetails(static_cast<USubUVComponent*>(Component));
	}

	if (Component->IsA(UBillboardComponent::StaticClass()))
	{
		DrawBillboardComponentDetials(static_cast<UBillboardComponent*>(Component), Engine);
	}
}

bool FPropertyWindow::AddComponentToActor(AActor* SelectedActor, UClass* ComponentClass, const char* BaseName)
{
	if (!SelectedActor || !ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return false;
	}

	USceneComponent* SelectedSceneComponent = GetSelectedSceneComponent(SelectedActor);
	const bool bIsSceneComponentClass = ComponentClass->IsChildOf(USceneComponent::StaticClass());
	if (bIsSceneComponentClass && !SelectedSceneComponent && SelectedActor->GetRootComponent())
	{
		return false;
	}

	const FString ComponentName = BuildUniqueComponentName(
		SelectedActor,
		(BaseName && BaseName[0] != '\0') ? FString(BaseName) : ComponentClass->GetName());

	UActorComponent* NewComponent = static_cast<UActorComponent*>(
		FObjectFactory::ConstructObject(ComponentClass, SelectedActor, ComponentName));
	if (!NewComponent)
	{
		return false;
	}

	SelectedActor->AddOwnedComponent(NewComponent);

	if (NewComponent->IsA(USceneComponent::StaticClass()))
	{
		USceneComponent* NewSceneComponent = static_cast<USceneComponent*>(NewComponent);
		if (SelectedSceneComponent)
		{
			NewSceneComponent->AttachTo(SelectedSceneComponent);
		}
		else
		{
			SelectedActor->SetRootComponent(NewSceneComponent);
		}
	}

	if (!NewComponent->IsRegistered())
	{
		NewComponent->OnRegister();
	}

	if (NewComponent->IsA(UPrimitiveComponent::StaticClass()))
	{
		UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(NewComponent);
		PrimitiveComponent->UpdateBounds();
	}

	if (NewComponent->IsA(UTextRenderComponent::StaticClass()))
	{
		UTextRenderComponent* TextComponent = static_cast<UTextRenderComponent*>(NewComponent);
		TextComponent->MarkTextMeshDirty();
	}

	if (ULevel* Level = SelectedActor->GetLevel())
	{
		Level->MarkSpatialDirty();
	}

	SelectedComponent = NewComponent;
	return true;
}

bool FPropertyWindow::DrawAddComponentButton(AActor* SelectedActor)
{
	if (!SelectedActor)
	{
		return false;
	}

	bool bAddedComponent = false;
	constexpr float AddButtonWidth = 90.0f;
	USceneComponent* SelectedSceneComponent = GetSelectedSceneComponent(SelectedActor);

	ImGui::SameLine();
	ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x > AddButtonWidth
		? ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - AddButtonWidth
		: ImGui::GetCursorPosX());

	if (ImGui::Button("+ Add", ImVec2(AddButtonWidth, 0.0f)))
	{
		ImGui::OpenPopup("##AddComponentPopup");
	}

	if (ImGui::BeginPopup("##AddComponentPopup"))
	{
		ImGui::TextDisabled("Add Component");
		if (SelectedComponent && IsComponentOwnedByActor(SelectedActor, SelectedComponent))
		{
			const FString ComponentName = SelectedComponent->GetName().empty()
				? SelectedComponent->GetClass()->GetName()
				: SelectedComponent->GetName();
			ImGui::Text("Target: %s", ComponentName.c_str());
		}
		else
		{
			ImGui::TextDisabled("Target: Select a component below");
		}
		ImGui::Separator();

		for (const FComponentAddOption& Option : GComponentAddOptions)
		{
			UClass* OptionClass = Option.GetClass();
			const bool bIsSceneOption = OptionClass && OptionClass->IsChildOf(USceneComponent::StaticClass());
			const bool bCanAdd = OptionClass && (!bIsSceneOption || SelectedSceneComponent || !SelectedActor->GetRootComponent());

			ImGui::BeginDisabled(!bCanAdd);
			if (ImGui::Selectable(Option.Label))
			{
				bAddedComponent = AddComponentToActor(SelectedActor, OptionClass, Option.BaseName);
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndDisabled();
		}

		ImGui::EndPopup();
	}

	return bAddedComponent;
}

void FPropertyWindow::DrawSceneComponentNode(USceneComponent* Component, int32 Depth)
{
	if (!Component)
	{
		return;
	}

	const FString ClassName = Component->GetClass() ? Component->GetClass()->GetName() : "USceneComponent";
	const FString ComponentName = Component->GetName().empty() ? ClassName : Component->GetName();
	const bool bIsRoot = (Component->GetAttachParent() == nullptr);
	const TArray<USceneComponent*>& Children = Component->GetAttachChildren();
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (SelectedComponent == Component)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (Children.empty())
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	const bool bOpen = ImGui::TreeNodeEx(
		Component,
		Flags,
		"%s%s (%s)",
		bIsRoot ? "[Root] " : "",
		ComponentName.c_str(),
		ClassName.c_str());

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		SelectedComponent = Component;
	}

	if (bOpen && !Children.empty())
	{
		for (USceneComponent* Child : Children)
		{
			DrawSceneComponentNode(Child, Depth + 1);
		}
		ImGui::TreePop();
	}
}

void FPropertyWindow::DrawNonSceneComponentEntry(UActorComponent* Component)
{
	if (!Component)
	{
		return;
	}

	const FString ClassName = Component->GetClass() ? Component->GetClass()->GetName() : "UActorComponent";
	const FString ComponentName = Component->GetName().empty() ? ClassName : Component->GetName();
	const FString Label = ComponentName + " (" + ClassName + ")";

	if (ImGui::Selectable(Label.c_str(), SelectedComponent == Component))
	{
		SelectedComponent = Component;
	}
}

void FPropertyWindow::DrawComponentSection(AActor* SelectedActor)
{
	if (!SelectedActor)
	{
		ImGui::TextDisabled("No actor selected.");
		return;
	}

	if (USceneComponent* RootComponent = SelectedActor->GetRootComponent())
	{
		DrawSceneComponentNode(RootComponent, 0);
	}
	else
	{
		ImGui::TextDisabled("No root scene component.");
	}

	bool bHasNonSceneComponent = false;
	for (UActorComponent* Component : SelectedActor->GetComponents())
	{
		if (!Component || Component->IsA(USceneComponent::StaticClass()))
		{
			continue;
		}

		if (!bHasNonSceneComponent)
		{
			ImGui::Spacing();
			ImGui::TextDisabled("Other Components");
			bHasNonSceneComponent = true;
		}

		DrawNonSceneComponentEntry(Component);
	}
}

void FPropertyWindow::Render(FEditorEngine* Engine)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	bool bOpen = ImGui::Begin("Properties");
	ImGui::PopStyleVar();

	if (!bOpen)
	{
		ImGui::End();
		return;
	}

	bModified = false;

	ImGui::TextDisabled("Selected:");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.4f, 1.0f), "%s", ActorNameBuf);

	AActor* SelectedActor = Engine ? Engine->GetSelectedActor() : nullptr;
	if (!IsComponentOwnedByActor(SelectedActor, SelectedComponent))
	{
		SelectedComponent = nullptr;
		if (SelectedActor)
		{
			if (USceneComponent* RootComponent = SelectedActor->GetRootComponent())
			{
				SelectedComponent = RootComponent;
			}
			else
			{
				for (UActorComponent* Component : SelectedActor->GetComponents())
				{
					if (Component)
					{
						SelectedComponent = Component;
						break;
					}
				}
			}
		}
	}

	ImGui::Separator();

	ImGui::TextDisabled("Components");
	DrawAddComponentButton(SelectedActor);

	const float AvailableHeight = ImGui::GetContentRegionAvail().y;
	const float ComponentsPanelHeight = AvailableHeight > 220.0f ? AvailableHeight * 0.4f : 120.0f;
	if (ImGui::BeginChild("##ComponentsPanel", ImVec2(0.0f, ComponentsPanelHeight), true))
	{
		DrawComponentSection(SelectedActor);
	}
	ImGui::EndChild();

	ImGui::Spacing();
	ImGui::TextDisabled("Details");
	if (ImGui::BeginChild("##DetailsPanel", ImVec2(0.0f, 0.0f), true))
	{
		DrawDetailsSection(SelectedComponent, Engine);
	}
	ImGui::EndChild();

	ImGui::End();
	return;

	if (SelectedActor && ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent(8.0f);
		DrawComponentSection(SelectedActor);
		ImGui::Unindent(8.0f);
	}

	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent(8.0f);
		DrawTransformSection();
		ImGui::Unindent(8.0f);
	}
	if (Engine)
	{
		if (SelectedActor)
		{
			if (ImGui::CollapsingHeader("Billboard", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Indent(8.0f);
				for (UActorComponent* Component : SelectedActor->GetComponents())
				{
					if (!Component) continue;

					if (Component->IsA(USubUVComponent::StaticClass()))
					{
						USubUVComponent* SubUVComp = static_cast<USubUVComponent*>(Component);
						bool bBillboard = SubUVComp->IsBillboard();
						if (ImGui::Checkbox("SubUV Billboard", &bBillboard))
							SubUVComp->SetBillboard(bBillboard);
					}
					else if (Component->IsA(UTextRenderComponent::StaticClass()) && !Component->IsA(UUUIDBillboardComponent::StaticClass()))
					{
						UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(Component);
						bool bBillboard = TextComp->IsBillboard();
						if (ImGui::Checkbox("Text Billboard", &bBillboard))
							TextComp->SetBillboard(bBillboard);
					}
				}
				ImGui::Unindent(8.0f);
			}
			if (UStaticMeshComponent* MeshComp = SelectedActor->GetComponentByClass<UStaticMeshComponent>())
			{
				if (ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Indent(8.0f);

					// 1. 현재 컴포넌트에 할당된 메쉬 정보 가져오기
					UStaticMesh* CurrentMesh = MeshComp->GetStaticMesh();
					std::string CurrentMeshName = CurrentMesh ? CurrentMesh->GetAssetPathFileName() : "None";

					ImGui::Text("Mesh Asset:");
					ImGui::SameLine();

					ImGui::PushItemWidth(200.f);
					if (ImGui::BeginCombo("##StaticMeshAssign", CurrentMeshName.c_str()))
					{
						// 2. TObjectIterator를 사용하여 로드된 모든 UStaticMesh를 순회
						for (TObjectIterator<UStaticMesh> It; It; ++It)
						{
							UStaticMesh* MeshAsset = It.Get();
							if (!MeshAsset) continue;

							std::string MeshName = MeshAsset->GetAssetPathFileName();
							bool bSelected = (CurrentMesh == MeshAsset);

							if (ImGui::Selectable(MeshName.c_str(), bSelected))
							{
								// 3. 선택 시 새로운 메쉬 할당
								MeshComp->SetStaticMesh(MeshAsset);
							}

							if (bSelected)
							{
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}
					ImGui::PopItemWidth();

					ImGui::Unindent(8.0f);
				}

				if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Indent(8.0f);

					if (UStaticMesh* MeshData = MeshComp->GetStaticMesh())
					{
						// 매니저에서 모든 머티리얼 리스트 가져오기
						TArray<FString> MatNames = FMaterialManager::Get().GetAllMaterialNames();
						uint32 NumSections = MeshData->GetNumSections();

						// ========================================================
						// [기능 1] 전체 섹션 머티리얼 일괄 변경
						// ========================================================
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
						ImGui::Text("Apply to All Sections:");
						ImGui::PopStyleColor();
						ImGui::SameLine();

						ImGui::PushItemWidth(180.f);
						if (ImGui::BeginCombo("##SetAllMaterials", "Select Material..."))
						{
							for (const FString& MatName : MatNames)
							{
								ImGui::PushID(MatName.c_str());

								auto ListMaterial = FMaterialManager::Get().FindByName(MatName);
								ImTextureID TexID = (ImTextureID)0; // 빨간줄 방지용 0 캐스팅

								if (ListMaterial && ListMaterial->GetMaterialTexture() && ListMaterial->GetMaterialTexture()->TextureSRV)
								{
									TexID = (ImTextureID)ListMaterial->GetMaterialTexture()->TextureSRV;
								}

								// 텍스처가 있으면 리스트에 썸네일 렌더링
								if (TexID)
								{
									ImGui::Image(TexID, ImVec2(24.0f, 24.0f));
									ImGui::SameLine();
									ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f); // 텍스트와 높이 맞춤
								}

								if (ImGui::Selectable(MatName.c_str(), false))
								{
									if (ListMaterial)
									{
										for (uint32 j = 0; j < NumSections; ++j)
										{
											MeshComp->SetMaterial(j, ListMaterial);
										}
									}
								}
								ImGui::PopID();
							}
							ImGui::EndCombo();
						}
						ImGui::PopItemWidth();

						float MasterScroll[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

						if (NumSections > 0)
						{
							if (std::shared_ptr<FMaterial> FirstMat = MeshComp->GetMaterial(0))
							{
								FirstMat->GetParameterData("UVScrollSpeed", MasterScroll, sizeof(MasterScroll));
							}
						}

						ImGui::PushItemWidth(180.f);
						// DragFloat2를 사용하므로 MasterScroll[0], MasterScroll[1] 값만 조작됩니다. (나머지 2개는 패딩 역할)
						if (ImGui::DragFloat2("Scroll All Sections", MasterScroll, 0.001f, -5.0f, 5.0f, "%.2f"))
						{
							for (uint32 j = 0; j < NumSections; ++j)
							{
								if (std::shared_ptr<FMaterial> Mat = MeshComp->GetMaterial(j))
								{
									Mat->SetParameterData("UVScrollSpeed", MasterScroll, sizeof(MasterScroll));
								}
							}
						}
						ImGui::PopItemWidth();

						ImGui::Separator();
						ImGui::Spacing();
						// ========================================================

						// 섹션 개수만큼 머티리얼 슬롯(콤보박스) 생성
						for (uint32 i = 0; i < NumSections; ++i)
						{
							std::shared_ptr<FMaterial> CurrentMat = MeshComp->GetMaterial(i);
							std::string CurrentMatName = CurrentMat ? CurrentMat->GetOriginName() : "None";

							ImGui::PushID(i); // ID 충돌 방지
							std::string Label = "Section " + std::to_string(i);

							ImGui::PushItemWidth(180.f); // 콤보박스 너비 조절

							// ========================================================
							// [기능 2] 개별 섹션 콤보박스 오픈 시 미리보기 출력
							// ========================================================
							if (ImGui::BeginCombo(Label.c_str(), CurrentMatName.c_str()))
							{
								for (const FString& MatName : MatNames)
								{
									ImGui::PushID(MatName.c_str());
									bool bSelected = (CurrentMatName == MatName);

									auto ListMaterial = FMaterialManager::Get().FindByName(MatName);
									ImTextureID TexID = (ImTextureID)0; // 빨간줄 방지용 0 캐스팅

									if (ListMaterial && ListMaterial->GetMaterialTexture() && ListMaterial->GetMaterialTexture()->TextureSRV)
									{
										TexID = (ImTextureID)ListMaterial->GetMaterialTexture()->TextureSRV;
									}

									// 텍스처가 있으면 리스트에 썸네일 렌더링
									if (TexID)
									{
										ImGui::Image(TexID, ImVec2(24.0f, 24.0f));
										ImGui::SameLine();
										ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f); // 텍스트와 높이 맞춤
									}

									if (ImGui::Selectable(MatName.c_str(), bSelected))
									{
										if (ListMaterial)
										{
											MeshComp->SetMaterial(i, ListMaterial);
										}
									}
									if (bSelected)
									{
										ImGui::SetItemDefaultFocus();
									}
									ImGui::PopID();
								}
								ImGui::EndCombo();
							}

							if (CurrentMat)
							{
								FVector4 MatColor = CurrentMat->GetVectorParameter("BaseColor");
								float ColorArray[4] = { MatColor.X, MatColor.Y, MatColor.Z, MatColor.W };

								ImGui::PushID(i + 1000);
								if (ImGui::ColorEdit4("Base Color", ColorArray))
								{
									CurrentMat->SetParameterData("BaseColor", ColorArray, sizeof(ColorArray));
								}
								ImGui::PopID();

								if (auto MatTex = CurrentMat->GetMaterialTexture())
								{
									float SpeedArray[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
									CurrentMat->GetParameterData("UVScrollSpeed", SpeedArray, sizeof(SpeedArray));

									ImGui::PushID(i + 2000);
									// 마찬가지로 UI 조작은 X, Y 2개만 합니다.
									if (ImGui::DragFloat2("UV Scroll", SpeedArray, 0.001f, -5.0f, 5.0f, "%.2f"))
									{
										CurrentMat->SetParameterData("UVScrollSpeed", SpeedArray, sizeof(SpeedArray));
									}
									ImGui::PopID();

								}
							}
							ImGui::PopID(); // PushID(i)에 대한 Pop
							ImGui::Spacing();
						}
					}
					else
					{
						ImGui::TextDisabled("No Static Mesh Assigned");
					}
					ImGui::Unindent(8.0f);
				}
			}

			if (UBillboardComponent* BillboardComp = SelectedActor->GetComponentByClass<UBillboardComponent>())
			{

			}
		}
	}
	ImGui::End();
}
