#include "GameFramework/TriggerVolumeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/World.h"
#include "Component/BoxComponent.h"
#include "Core/CollisionTypes.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(ATriggerVolumeBase, AActor)

void ATriggerVolumeBase::InitDefaultComponents(const FVector& Extent)
{
	TriggerBox = AddComponent<UBoxComponent>();
	SetRootComponent(TriggerBox);

	TriggerBox->SetBoxExtent(Extent);
	// Overlap-only — 물리적으로 충돌하지 않고 진입만 감지.
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECollisionChannel::Trigger);
	TriggerBox->SetCollisionResponseToAllChannels(ECollisionResponse::Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
}

void ATriggerVolumeBase::PostDuplicate()
{
	Super::PostDuplicate();
	TriggerBox = Cast<UBoxComponent>(GetRootComponent());
}

void ATriggerVolumeBase::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	Super::GetEditableProperties(OutProps);
	OutProps.push_back({ "TriggerTag", EPropertyType::Name, "Trigger", &TriggerTag });
}

void ATriggerVolumeBase::BeginPlay()
{
	Super::BeginPlay();

	// 직렬화 경로(씬 로드)에서 PostDuplicate가 안 거쳐졌을 수 있어 한 번 더 잡는다.
	if (!TriggerBox)
	{
		TriggerBox = Cast<UBoxComponent>(GetRootComponent());
	}

	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddRaw(this, &ATriggerVolumeBase::HandleBeginOverlap);
		TriggerBox->OnComponentEndOverlap.AddRaw(this, &ATriggerVolumeBase::HandleEndOverlap);
	}
}

void ATriggerVolumeBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << TriggerTag;
}

void ATriggerVolumeBase::HandleBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	// Possessed Pawn만 통과 — 자유 비행 중인 Pawn / 비-Pawn 액터는 무시
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPossessed()) return;

	OnPossessedPawnEntered(Pawn);

	if (UWorld* W = GetWorld())
	{
		if (AGameModeBase* GM = W->GetGameMode())
		{
			GM->OnPossessedPawnEnteredTrigger(this, Pawn);
		}
	}
}

void ATriggerVolumeBase::HandleEndOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPossessed()) return;

	OnPossessedPawnExited(Pawn);

	if (UWorld* W = GetWorld())
	{
		if (AGameModeBase* GM = W->GetGameMode())
		{
			GM->OnPossessedPawnExitedTrigger(this, Pawn);
		}
	}
}
