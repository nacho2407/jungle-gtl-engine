#include "TextRenderComponent.h"

#include "GameFramework/AActor.h"
#include "Core/ResourceManager.h"
#include "Render/Scene/RenderCommand.h"

DEFINE_CLASS(UTextRenderComponent, UPrimitiveComponent)
REGISTER_FACTORY(UTextRenderComponent)

void UTextRenderComponent::SetFont(const FName& InFontName)
{
	FontName = InFontName;
	CachedFont = FResourceManager::Get().FindFont(FontName);
}

bool UTextRenderComponent::GetRenderCommand(FRenderCommand& OutCommand)
{
	if (!bIsVisible || Text.empty()) return false;
	if (!CachedFont || !CachedFont->IsLoaded()) return false;

	OutCommand.Type = ERenderCommandType::Font;
	OutCommand.PerObjectConstants.Model = GetWorldMatrix();
	OutCommand.PerObjectConstants.Color = Color;
	OutCommand.TextData     = Text;
	OutCommand.AtlasResource = CachedFont;
	OutCommand.SpriteSize.X = FontSize; // X = Scale
	return true;
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
