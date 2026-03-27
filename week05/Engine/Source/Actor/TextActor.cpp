#include "TextActor.h"
#include "Component/TextComponent.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(ATextActor, AActor)

void ATextActor::PostSpawnInitialize()
{
	TextComponent = FObjectFactory::ConstructObject<UTextComponent>(this, "TextComponent");
	AddOwnedComponent(TextComponent);

	SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

	if (TextComponent)
	{
		TextComponent->SetText("Text");
		TextComponent->SetBillboard(false);
		TextComponent->SetTextColor(FVector4(1.0f, 1.0f, 1.0f, 1.0f));

		FTransform RelativeTransform = TextComponent->GetRelativeTransform();
		RelativeTransform.SetScale3D(FVector(0.3f, 0.3f, 0.3f));
		TextComponent->SetRelativeTransform(RelativeTransform);
	}

	AActor::PostSpawnInitialize();
}