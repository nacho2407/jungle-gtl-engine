#include "GameFramework/Actor/PlayerStart.h"

#include "Component/SceneComponent.h"

void APlayerStart::InitDefaultComponents()
{
	if (!RootSceneComponent)
	{
		RootSceneComponent = AddComponent<USceneComponent>();
		SetRootComponent(RootSceneComponent);
	}
}

void APlayerStart::BeginPlay()
{
	// scene 파일에 root가 없는 legacy spawn 경로 보정
	if (!GetRootComponent())
	{
		InitDefaultComponents();
	}

	Super::BeginPlay();
}

void APlayerStart::PostDuplicate()
{
	Super::PostDuplicate();
	RootSceneComponent = GetRootComponent();
}
