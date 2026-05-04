#pragma once

#include "Game/Pawn/CarPawn.h"

class APawn;
class UPrimitiveComponent;
class UPointLightComponent;
struct FHitResult;

// ============================================================
// APoliceCar — EscapePolice 페이즈에서 동적 spawn되는 추적 차량.
//
// ACarPawn 상속 — 섀시/이동/충돌은 그대로 재활용.
// 차이점:
//   - bAutoPossessPlayer = false (GameMode가 player를 possess할 후보로 잡지 않게)
//   - LuaScript = "PoliceCarAI.lua" — Tick에서 throttle/steering 합성
//   - LuaCameraScript 제거 — F2 토글 등 카메라 입력이 player와 충돌하지 않도록
//   - CollisionBox 의 OnComponentHit 바인딩 — ACarPawn(possessed)과 충돌 시 잡힘 처리
//
// AI 추적 대상은 GameMode가 SetTarget 으로 주입한다.
// ============================================================
class APoliceCar : public ACarPawn
{
public:
	DECLARE_CLASS(APoliceCar, ACarPawn)

	APoliceCar() = default;
	~APoliceCar() override = default;

	// 코드 spawn 시 호출 — ACarPawn::InitDefaultComponents 위에 police 전용 셋업을 얹는다.
	void InitDefaultPoliceComponents();

	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	void SetTarget(APawn* InTarget) { Target = InTarget; }
	APawn* GetTarget() const { return Target; }

private:
	void HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	APawn* Target = nullptr;
	bool   bAlreadyCaught = false;  // OnPlayerCaught를 한 번만 호출하기 위한 가드 (A→B / B→A 양쪽 hit 발생 대비)

	// 사이렌 — 차체 위 좌/우 PointLight 두 개. Tick 에서 sin 파동으로 교차 점멸 (red/blue).
	UPointLightComponent* LeftSiren  = nullptr;
	UPointLightComponent* RightSiren = nullptr;
	float SirenTime = 0.0f;

	static constexpr float SirenBlinkRate     = 4.0f;   // Hz — 초당 4번 사이클 (좌/우 각 2번씩 점멸)
	static constexpr float SirenMaxIntensity  = 30.0f;
	static constexpr float SirenAttenRadius   = 6.0f;
};
