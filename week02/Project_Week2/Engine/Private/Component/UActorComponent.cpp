#include "Component/UActorComponent.h"
#include "Actor/AActor.h"

UActorComponent::UActorComponent()
	: UObject()
	, Owner(nullptr)
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

void UActorComponent::TickComponent(float DeltaTime) // @@@ ��..���ִ°���
{
	// Tick�� ��Ȱ��ȭ�Ǿ� �ְų�, ������Ʈ�� ��Ȱ�� ���¸� ���� �� ��
	if (!bCanEverTick || !bIsActivate)
	{
		return;
	}
}

void UActorComponent::Activate()
{
	bIsActivate = true;

	// ������ �̹� ���۵Ǿ��µ� ���� BeginPlay�� ȣ����� �ʾҴٸ� ȣ��
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