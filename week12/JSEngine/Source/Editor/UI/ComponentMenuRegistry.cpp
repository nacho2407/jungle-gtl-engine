#include "Editor/UI/ComponentMenuRegistry.h"

#include "Component/ActorComponent.h"
#include "Core/Reflection/ReflectionRegistry.h"
#include "ImGui/imgui.h"
#include "Object/Class.h"

#include <algorithm>
#include <cstring>

namespace
{
	bool IsSpawnableComponentClass(const UClass* Class);
	bool SortComponentClassForMenu(const UClass* Lhs, const UClass* Rhs);
}

void FComponentMenuRegistry::CollectSpawnableComponentClasses(TArray<UClass*>& OutClasses)
{
	OutClasses.clear();
	FReflectionRegistry::Get().GetClassesDerivedFrom(UActorComponent::StaticClass(), OutClasses);
	OutClasses.erase(
		std::remove_if(
			OutClasses.begin(),
			OutClasses.end(),
			[](const UClass* Class)
			{
				return !IsSpawnableComponentClass(Class);
			}),
		OutClasses.end());
	std::stable_sort(OutClasses.begin(), OutClasses.end(), SortComponentClassForMenu);
}

UClass* FComponentMenuRegistry::DrawSpawnableComponentClassMenu()
{
	TArray<UClass*> ComponentClasses;
	CollectSpawnableComponentClasses(ComponentClasses);

	if (ComponentClasses.empty())
	{
		ImGui::TextDisabled("No spawnable components");
		return nullptr;
	}

	UClass* SelectedClass = nullptr;
	const char* CurrentCategory = nullptr;
	bool bCategoryMenuOpen = false;
	for (UClass* ComponentClass : ComponentClasses)
	{
		if (!ComponentClass)
		{
			continue;
		}

		const char* Category = GetClassMenuCategory(ComponentClass);
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

		if (ImGui::Selectable(ComponentClass->GetDisplayName()))
		{
			SelectedClass = ComponentClass;
		}
	}

	if (bCategoryMenuOpen)
	{
		ImGui::EndMenu();
	}

	return SelectedClass;
}

const char* FComponentMenuRegistry::GetClassMenuCategory(const UClass* Class)
{
	const char* Category = Class ? Class->GetCategory() : nullptr;
	return (Category && Category[0] != '\0') ? Category : "Misc";
}

namespace
{
	bool IsSpawnableComponentClass(const UClass* Class)
	{
		return Class
			&& Class->IsChildOf(UActorComponent::StaticClass())
			&& Class->HasAnyClassFlags(CF_SpawnableComponent)
			&& !Class->HasAnyClassFlags(CF_Abstract);
	}

	bool SortComponentClassForMenu(const UClass* Lhs, const UClass* Rhs)
	{
		const char* LhsCategory = FComponentMenuRegistry::GetClassMenuCategory(Lhs);
		const char* RhsCategory = FComponentMenuRegistry::GetClassMenuCategory(Rhs);
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
