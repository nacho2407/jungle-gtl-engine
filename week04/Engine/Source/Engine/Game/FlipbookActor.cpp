#include "Core/CoreMinimal.h"
#include "FlipbookActor.h"
#include "Engine/Component/Sprite/SubUVAnimatedComponent.h"
#include "Engine/Component/Core/PrimitiveComponent.h" 

namespace
{
    FColor ToColor(const FVector4& InColor)
    {
        return FColor(InColor.X, InColor.Y, InColor.Z, InColor.W);
    }
} // namespace

AFlipbookActor::AFlipbookActor()
{
    auto* AnimatedComponent = new Engine::Component::USubUVAnimatedComponent();
    AnimatedComponent->SetColor({0.8f, 0.8f, 0.8f, 1.f});
    AddOwnedComponent(AnimatedComponent, true);

    Name = "FlipbookActor";

    UE_LOG(FEditor, ELogVerbosity::Log, "FlipbookActor Component RTTI: %s",
           AnimatedComponent->GetTypeName());
}

Engine::Component::USubUVAnimatedComponent* AFlipbookActor::GetSubUVAnimatedComponent() const
{
    return Cast<Engine::Component::USubUVAnimatedComponent>(RootComponent);
}

bool AFlipbookActor::IsRenderable() const { return GetPrimitiveComponent() != nullptr; }

bool AFlipbookActor::IsSelected() const
{
    if (RootComponent == nullptr)
    {
        return false;
    }

    return RootComponent->IsSelected();
}

FColor AFlipbookActor::GetColor() const
{
    if (const auto* PrimitiveComponent = GetPrimitiveComponent())
    {
        return PrimitiveComponent->GetColor();
    }

    return FColor::White();
}

EBasicMeshType AFlipbookActor::GetMeshType() const
{
    // 변수명 CubeComponent -> PrimitiveComp 로 가독성 수정
    if (const auto* PrimitiveComp = GetPrimitiveComponent())
    {
        return PrimitiveComp->GetBasicMeshType();
    }

    return AActor::GetMeshType();
}

uint32 AFlipbookActor::GetObjectId() const { return UUID; }

Engine::Component::UPrimitiveComponent* AFlipbookActor::GetPrimitiveComponent() const
{
    return Cast<Engine::Component::UPrimitiveComponent>(RootComponent);
}

REGISTER_CLASS(, AFlipbookActor)