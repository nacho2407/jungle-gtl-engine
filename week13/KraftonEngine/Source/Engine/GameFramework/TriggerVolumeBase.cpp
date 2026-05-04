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
	// 신규 spawn (World->SpawnActor<ATriggerVolumeBase>()) 경로 — 호출자가 컴포넌트를 채울
	// 시점이 없어 RootComponent 가 비어있다. AMeteor / APoliceCar 와 동일 패턴으로 여기서
	// 직접 default 셋업. 직렬화/Duplicate 경로에선 RootComponent 가 deserialize 돼 있어
	// InitDefaultComponents 안 부르고 cast 만.
	if (!GetRootComponent())
	{
		InitDefaultComponents();
	}

	if (!TriggerBox)
	{
		TriggerBox = Cast<UBoxComponent>(GetRootComponent());
	}

	// 씬에 잘못 직렬화된 값(GenerateOverlapEvents=false 등)이 있으면 PhysX가 trigger flag
	// 대신 simulation shape로 등록 → Pawn과 일반 충돌 발생 → trigger 표면에서 튕김.
	// Super::BeginPlay 전에 강제 보정해야 PrimitiveComponent::BeginPlay → PhysX
	// RegisterComponent 시점에 trigger 셋업이 반영된다.
	if (TriggerBox)
	{
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		TriggerBox->SetCollisionObjectType(ECollisionChannel::Trigger);
		TriggerBox->SetCollisionResponseToAllChannels(ECollisionResponse::Overlap);
		TriggerBox->SetGenerateOverlapEvents(true);
		TriggerBox->SetSimulatePhysics(false);
	}

	Super::BeginPlay();

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
