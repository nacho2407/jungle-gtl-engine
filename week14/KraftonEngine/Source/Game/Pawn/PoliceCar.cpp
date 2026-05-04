#include "Game/Pawn/PoliceCar.h"
#include "Game/GameMode/GameModeCarGame.h"

#include "Component/BoxComponent.h"
#include "Component/LuaScriptComponent.h"
#include "Component/Light/PointLightComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/World.h"
#include "Math/Vector.h"
#include "Core/Log.h"

#include <cmath>

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

	// 사이렌 — 차체 위 좌/우에 PointLight 두 개. 차체 CollisionBox extent 가 (2.0, 1.0, 0.5) 이라
	// Y 방향 ±0.6, Z+0.7 위치에 둠. 색은 빨강/파랑 (전형적인 경찰차).
	if (UBoxComponent* Box = GetCollisionBox())
	{
		LeftSiren = AddComponent<UPointLightComponent>();
		LeftSiren->AttachToComponent(Box);
		LeftSiren->SetRelativeLocation(FVector(0.0f, -0.4f, 1.2f));
		LeftSiren->SetLightColor(FVector4(1.0f, 0.05f, 0.05f, 1.0f));   // red
		LeftSiren->SetAttenuationRadius(SirenAttenRadius);
		LeftSiren->SetIntensity(0.0f);

		RightSiren = AddComponent<UPointLightComponent>();
		RightSiren->AttachToComponent(Box);
		RightSiren->SetRelativeLocation(FVector(0.0f, 0.4f, 1.2f));
		RightSiren->SetLightColor(FVector4(0.1f, 0.3f, 1.0f, 1.0f));    // blue
		RightSiren->SetAttenuationRadius(SirenAttenRadius);
		RightSiren->SetIntensity(0.0f);
	}
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

void APoliceCar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 사이렌 점멸 — 좌/우 PointLight 가 sin 파동으로 교차 점멸. 음수 영역은 0 으로 clamp 해
	// off → on → off 의 절반 cycle 형태. PI 만큼 phase shift 해서 좌가 켜질 때 우는 꺼짐.
	if (!LeftSiren && !RightSiren) return;

	SirenTime += DeltaTime;
	const float Phase = SirenTime * SirenBlinkRate * 6.28318530718f;
	const float LeftWave  = std::sin(Phase);
	const float RightWave = std::sin(Phase + 3.14159265359f);
	const float LeftI  = (LeftWave  > 0.0f ? LeftWave  : 0.0f) * SirenMaxIntensity;
	const float RightI = (RightWave > 0.0f ? RightWave : 0.0f) * SirenMaxIntensity;

	if (LeftSiren)  LeftSiren->SetIntensity(LeftI);
	if (RightSiren) RightSiren->SetIntensity(RightI);
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
