#include "Editor/UI/EditorControlWidget.h"

#include "Editor/EditorEngine.h"
#include "Camera/ViewportCamera.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"
#include "ImGui/imgui.h"
#include "Object/Class.h"

#include <algorithm>
#include <cstring>

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

namespace
{
	const char* GetClassMenuCategory(const UClass* Class);
	bool HasPlayerStart(UWorld* World);
	bool IsPlaceableActorClass(const UClass* Class);
	bool SortActorClassForPlacement(const UClass* Lhs, const UClass* Rhs);
}

void FEditorControlWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
	SelectedPrimitiveType = 0;
	bPlaceableActorCacheDirty = true;
}

const char* FEditorControlWidget::GetPrimitiveTypeLabel(int32 PrimitiveType) const
{
	UClass* Class = GetPrimitiveTypeClass(PrimitiveType);
	if (!Class)
	{
		return "";
	}
	return Class->GetDisplayName();
}

int32 FEditorControlWidget::GetPrimitiveTypeCount() const
{
	RefreshPlaceableActorCache();
	return static_cast<int32>(PlaceableActorClasses.size());
}

void FEditorControlWidget::RefreshPlaceableActorCache() const
{
	if (!bPlaceableActorCacheDirty)
	{
		return;
	}

	PlaceableActorClasses.clear();
	FReflectionRegistry::Get().GetClassesDerivedFrom(AActor::StaticClass(), PlaceableActorClasses);
	PlaceableActorClasses.erase(
		std::remove_if(
			PlaceableActorClasses.begin(),
			PlaceableActorClasses.end(),
			[](const UClass* Class)
			{
				return !IsPlaceableActorClass(Class);
			}),
		PlaceableActorClasses.end());

	std::stable_sort(
		PlaceableActorClasses.begin(),
		PlaceableActorClasses.end(),
		SortActorClassForPlacement);

	bPlaceableActorCacheDirty = false;
}

UClass* FEditorControlWidget::GetPrimitiveTypeClass(int32 PrimitiveType) const
{
	RefreshPlaceableActorCache();
	if (PrimitiveType < 0 || PrimitiveType >= static_cast<int32>(PlaceableActorClasses.size()))
	{
		return nullptr;
	}
	return PlaceableActorClasses[PrimitiveType];
}

bool FEditorControlWidget::DrawPlaceActorMenu(const FVector& SpawnPoint, bool bClosePopupOnSpawn)
{
	bool bSpawned = false;
	RefreshPlaceableActorCache();

	const char* CurrentCategory = nullptr;
	bool bCategoryMenuOpen = false;
	for (int32 PrimitiveType = 0; PrimitiveType < static_cast<int32>(PlaceableActorClasses.size()); ++PrimitiveType)
	{
		UClass* Class = PlaceableActorClasses[PrimitiveType];
		if (!Class)
		{
			continue;
		}

		const char* Category = GetClassMenuCategory(Class);
		if (!CurrentCategory || std::strcmp(Category, CurrentCategory) != 0)
		{
			if (bCategoryMenuOpen)
			{
				ImGui::EndMenu();
			}
			CurrentCategory = Category;
			bCategoryMenuOpen = ImGui::BeginMenu(CurrentCategory);
		}

		if (!bCategoryMenuOpen)
		{
			continue;
		}

		if (ImGui::MenuItem(Class->GetDisplayName()))
		{
			bSpawned = SpawnPrimitive(PrimitiveType, SpawnPoint, 1) || bSpawned;
			if (bSpawned && bClosePopupOnSpawn)
			{
				ImGui::CloseCurrentPopup();
			}
		}
	}

	if (bCategoryMenuOpen)
	{
		ImGui::EndMenu();
	}

	return bSpawned;
}

bool FEditorControlWidget::SpawnPrimitive(int32 PrimitiveType, const FVector& SpawnPoint, int32 Count)
{
	if (!EditorEngine)
	{
		return false;
	}

	UClass* ActorClass = GetPrimitiveTypeClass(PrimitiveType);
	if (!ActorClass)
	{
		return false;
	}

	UWorld* World = EditorEngine->GetFocusedWorld();
	if (!World)
	{
		return false;
	}

	Count = MathUtil::Clamp(Count, 1, 100);
	const bool bSpawningPlayerStart = ActorClass->IsChildOf(APlayerStart::StaticClass());
	if (bSpawningPlayerStart)
	{
		Count = 1;
		if (HasPlayerStart(World))
		{
			EditorEngine->GetNotificationService().Info("Player Start already exists");
			return false;
		}
	}

	EditorEngine->GetUndoSystem().CaptureSnapshot("Place Actor");
	bool bSpawnedAny = false;
	for (int32 i = 0; i < Count; i++)
	{
		AActor* Actor = World->SpawnActorByTypeName(ActorClass->GetName());
		if (!Actor)
		{
			continue;
		}

		if (bSpawningPlayerStart)
		{
			Actor->SetFName(FName("Player Start"));
		}
		Actor->SetActorLocation(SpawnPoint);
		bSpawnedAny = true;
	}

	return bSpawnedAny;
}

void FEditorControlWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!EditorEngine)
	{
		return;
	}

	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(360.0f, 180.0f), ImGuiCond_Once);

	ImGui::Begin("Jungle Control Panel");

	ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);

	FViewportCamera* Camera = EditorEngine->GetCamera();
	if (Camera == nullptr)
	{
		ImGui::End();
		return;
	}

	FVector CamPos = Camera->GetLocation();
	float CameraLocation[3] = { CamPos.X, CamPos.Y, CamPos.Z };
	if (ImGui::DragFloat3("Camera Location", CameraLocation, 0.1f, 0.0f, 0.0f, "%.1f"))
	{
		Camera->SetLocation(FVector(CameraLocation[0], CameraLocation[1], CameraLocation[2]));
	}

	FVector CamRot = Camera->GetRotation().Rotator().Euler();
	float CameraRotation[3] = { CamRot.X, CamRot.Y, CamRot.Z };
	if (ImGui::DragFloat3("Camera Rotation", CameraRotation, 0.1f, 0.0f, 0.0f, "%.1f"))
	{
		FRotator NewRotation = FRotator::MakeFromEuler(FVector(CameraRotation[0], CameraRotation[1], CameraRotation[2]));

		NewRotation.Normalize();
		Camera->SetRotation(NewRotation);
	}

	ImGui::PopItemWidth();

	SEPARATOR();

	ImGui::End();
}

namespace
{
	const char* GetClassMenuCategory(const UClass* Class)
	{
		const char* Category = Class ? Class->GetCategory() : nullptr;
		return (Category && Category[0] != '\0') ? Category : "Misc";
	}

	bool HasPlayerStart(UWorld* World)
	{
		if (!World)
		{
			return false;
		}

		for (AActor* Actor : World->GetActors())
		{
			if (Actor && Actor->IsA<APlayerStart>())
			{
				return true;
			}
		}
		return false;
	}

	bool IsPlaceableActorClass(const UClass* Class)
	{
		return Class && Class->IsChildOf(AActor::StaticClass()) && Class->HasAnyClassFlags(CF_Placeable) && !Class->HasAnyClassFlags(CF_Abstract);
	}

	bool SortActorClassForPlacement(const UClass* Lhs, const UClass* Rhs)
	{
		const char* LhsCategory = GetClassMenuCategory(Lhs);
		const char* RhsCategory = GetClassMenuCategory(Rhs);
		const int32 CategoryOrder = std::strcmp(LhsCategory, RhsCategory);
		if (CategoryOrder != 0)
		{
			return CategoryOrder < 0;
		}

		const char* LhsLabel = Lhs ? Lhs->GetDisplayName() : "";
		const char* RhsLabel = Rhs ? Rhs->GetDisplayName() : "";
		return std::strcmp(LhsLabel, RhsLabel) < 0;
	}
}
