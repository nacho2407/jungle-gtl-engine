#include "TextRenderComponent.h"

#include <cstring>
#include "GameFramework/AActor.h"
#include "Core/ResourceManager.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UTextRenderComponent, UPrimitiveComponent)
REGISTER_FACTORY(UTextRenderComponent)

void UTextRenderComponent::SetFont(const FName& InFontName)
{
	FontName = InFontName;
	CachedFont = FResourceManager::Get().FindFont(FontName);
}

FString UTextRenderComponent::GetOwnerUUIDToString() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return FName::None.ToString();
	}
	return std::to_string(OwnerActor->GetUUID());
}

FString UTextRenderComponent::GetOwnerNameToString() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return FName::None.ToString();
	}

	FName Name = OwnerActor->GetFName();
	if (Name.IsValid())
	{
		return Name.ToString();
	}
	return FName::None.ToString();
}

void UTextRenderComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Text", EPropertyType::String, &Text });
	OutProps.push_back({ "Font", EPropertyType::Name, &FontName });
	//OutProps.push_back({ "Color", EPropertyType::Vec4, &Color });
	OutProps.push_back({ "Font Size", EPropertyType::Float, &FontSize, 0.1f, 100.0f, 0.1f });
	OutProps.push_back({ "Visible", EPropertyType::Bool, &bIsVisible });
}

void UTextRenderComponent::PostEditProperty(const char* PropertyName)
{
	if (strcmp(PropertyName, "Font") == 0)
	{
		SetFont(FontName);
	}
}
