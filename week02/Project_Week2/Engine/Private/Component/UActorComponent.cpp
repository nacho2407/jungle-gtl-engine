#include "Component/UActorComponent.h"
#include "Actor/AActor.h"

UActorComponent::UActorComponent()
	: Owner(nullptr)
	, bCanEverTick(true)
	, bIsActivate(true)
	, bHasBegunPlay(false)
{
}

UActorComponent::~UActorComponent() {}

void UActorComponent::BeginPlay()
{
	bHasBegunPlay = true;
}
void UActorComponent::TickComponent(float DeltaTime) // @@@ 음..왜있는거임
{
	// Tick이 비활성화되어 있거나, 컴포넌트가 비활성 상태면 실행 안 함
	if (!bCanEverTick || !bIsActivate)
	{
		return;
	}
}

void UActorComponent::Activate()
{
	bIsActivate = true;

	// 게임이 이미 시작되었는데 아직 BeginPlay가 호출되지 않았다면 호출
	if (!bHasBegunPlay)
	{
		BeginPlay();
	}
}

void UActorComponent::Deactivate()
{
	bIsActivate = false;
}

void UActorComponent::SetOwner(AActor* InOwner)
{
	Owner = InOwner;
}
AActor* UActorComponent::GetOwner() const
{
	return Owner;
}