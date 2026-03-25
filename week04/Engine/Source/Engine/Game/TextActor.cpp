#include "TextActor.h"

#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Engine/Component/Text/AtlasTextComponent.h"

ATextActor::ATextActor()
{
    auto* TextComponent = new Engine::Component::UAtlasTextComponent();
    TextComponent->SetColor({0.8f, 0.8f, 0.8f, 1.f});
    AddOwnedComponent(TextComponent, true);

    Name = "TextActor";
}

Engine::Component::UAtlasTextComponent* ATextActor::GetTextComponent() const
{
    return Cast<Engine::Component::UAtlasTextComponent>(RootComponent);
}

bool ATextActor::IsRenderable() const { return GetPrimitiveComponent() != nullptr; }

bool ATextActor::IsSelected() const
{
    if (RootComponent == nullptr)
    {
        return false;
    }

    return RootComponent->IsSelected();
}

FColor ATextActor::GetColor() const
{
    if (const auto* PrimitiveComponent = GetPrimitiveComponent())
    {
        return PrimitiveComponent->GetColor();
    }

    return FColor::White();
}

EBasicMeshType ATextActor::GetMeshType() const
{
    if (const auto* TextComponent = GetTextComponent())
    {
        return TextComponent->GetBasicMeshType();
    }

    return AActor::GetMeshType();
}

uint32 ATextActor::GetObjectId() const { return UUID; }

Engine::Component::UPrimitiveComponent* ATextActor::GetPrimitiveComponent() const
{
    return Cast<Engine::Component::UPrimitiveComponent>(RootComponent);
}

REGISTER_CLASS(, ATextActor)
