#include "Game/Pawn/PoliceCar.h"
#include "Game/GameMode/GameModeCarGame.h"

#include "Component/BoxComponent.h"
#include "Component/LuaScriptComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/World.h"
#include "Core/Log.h"

IMPLEMENT_CLASS(APoliceCar, ACarPawn)

void APoliceCar::InitDefaultPoliceComponents()
{
	// 부모 셋업을 그대로 사용 — 단, LuaCameraScriptFile 은 빈 문자열로 넘겨도
	// 부모가 ULuaScriptComponent 를 추가하긴 하므로(EnsureDefaultScriptFile 경로),
	// 추가 후 RemoveComponent 로 해당 컴포넌트를 제거한다.
	Super::InitDefaultComponents("Data/Truck/TruckBody.obj", "PoliceCarAI.lua", "");

	// 카메라 Lua 제거 — F2 토글 등 player 카메라 입력과 충돌 방지
	if (ULuaScriptComponent* CamLua = GetLuaCameraScript())
	{
		RemoveComponent(CamLua);
	}

	// AI 차량은 player가 자동 Possess하지 않도록
	SetAutoPossessPlayer(false);
}

void APoliceCar::BeginPlay()
{
	// Spawn 경로(World::SpawnActor → AddActor)는 컴포넌트 셋업 전에 BeginPlay를 호출한다.
	// AMeteor 와 동일하게 lazy init — Super::BeginPlay 전에 컴포넌트를 채워야
	// AActor::BeginPlay 가 OwnedComponents 를 순회하면서 LuaScript / Primitive 의
	// BeginPlay(=Lua 로드, PhysX 등록)를 빠짐없이 호출한다.
	if (!GetCollisionBox())
	{
		InitDefaultPoliceComponents();
	}

	Super::BeginPlay();

	if (UBoxComponent* Box = GetCollisionBox())
	{
		Box->OnComponentHit.AddRaw(this, &APoliceCar::HandleHit);
	}
}

void APoliceCar::HandleHit(UPrimitiveComponent* /*HitComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, FVector /*Impulse*/, const FHitResult& /*Hit*/)
{
	if (bAlreadyCaught) return;

	// 잡힘 판정: 상대가 ACarPawn 이면서 possessed 인 경우만 (다른 경찰차/지면 등은 무시)
	APawn* OtherPawn = Cast<APawn>(OtherActor);
	if (!OtherPawn || !OtherPawn->IsPossessed()) return;

	// 자기 자신 또는 다른 APoliceCar 와의 충돌은 게임오버가 아님
	if (OtherActor->IsA<APoliceCar>()) return;

	bAlreadyCaught = true;

	UE_LOG("[Police] HandleHit other=%s", OtherActor->GetFName().ToString().c_str());

	if (UWorld* W = GetWorld())
	{
		if (auto* GM = Cast<AGameModeCarGame>(W->GetGameMode()))
		{
			GM->OnPlayerCaught(this);
		}
	}
}
