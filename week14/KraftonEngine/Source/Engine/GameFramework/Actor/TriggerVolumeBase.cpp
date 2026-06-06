#include "GameFramework/Actor/TriggerVolumeBase.h"
#include "GameFramework/Pawn/Pawn.h"
#include "Component/Shape/BoxComponent.h"
#include "Core/Types/CollisionTypes.h"
#include "Lua/LuaScriptManager.h"
#include "Serialization/Archive.h"

void ATriggerVolumeBase::InitDefaultComponents(const FVector& Extent)
{
	TriggerBox = AddComponent<UBoxComponent>();
	SetRootComponent(TriggerBox);

	TriggerBox->SetBoxExtent(Extent);
	// Overlap-only trigger shape 설정
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

void ATriggerVolumeBase::BeginPlay()
{
	// 코드 spawn 경로의 기본 컴포넌트 보정
	if (!GetRootComponent())
	{
		InitDefaultComponents();
	}

	if (!TriggerBox)
	{
		TriggerBox = Cast<UBoxComponent>(GetRootComponent());
	}

	// 씬 직렬화 값이 잘못된 경우에도 PhysX trigger flag가 유지되도록 BeginPlay 전에 보정합니다.
	if (TriggerBox)
	{
		TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		TriggerBox->SetCollisionObjectType(ECollisionChannel::Trigger);
		TriggerBox->SetCollisionResponseToAllChannels(ECollisionResponse::Overlap);
		TriggerBox->SetGenerateOverlapEvents(true);
	}

	Super::BeginPlay();

	if (TriggerBox)
	{
		TriggerBox->OnComponentBeginOverlap.AddRaw(this, &ATriggerVolumeBase::HandleBeginOverlap);
		TriggerBox->OnComponentEndOverlap.AddRaw(this, &ATriggerVolumeBase::HandleEndOverlap);
	}
}

void ATriggerVolumeBase::HandleBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	// Pawn overlap만 gameplay event로 전달합니다.
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn) return;
	if (bOnlyPossessedPawn && !Pawn->IsPossessed()) return;

	OnPossessedPawnEntered(Pawn);
	FLuaScriptManager::EmitGameEvent_Trigger("TriggerEnter", this, Pawn, TriggerTag.ToString());
}

void ATriggerVolumeBase::HandleEndOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn) return;
	if (bOnlyPossessedPawn && !Pawn->IsPossessed()) return;

	OnPossessedPawnExited(Pawn);
	FLuaScriptManager::EmitGameEvent_Trigger("TriggerExit", this, Pawn, TriggerTag.ToString());
}
