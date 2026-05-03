#include "Game/Component/CarDirtComponent.h"

#include "Game/Component/DirtComponent.h"
#include "GameFramework/AActor.h"
#include "Materials/MaterialManager.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UCarDirtComponent, USceneComponent)


void UCarDirtComponent::BeginPlay()
{
	USceneComponent::BeginPlay();
}

int32 UCarDirtComponent::CountDirtChildren() const
{
	int32 Count = 0;
	for (USceneComponent* Child : GetChildren())
	{
		if (Cast<UDirtComponent>(Child))
		{
			++Count;
		}
	}
	return Count;
}
