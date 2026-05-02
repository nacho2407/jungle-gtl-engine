#pragma once

#include "GameFramework/Pawn.h"

class UBoxComponent;
class UStaticMeshComponent;
class USphereComponent;
class ULuaScriptComponent;
class UCameraComponent;
class UCarMovementComponent;

// ============================================================
// ACarPawn — 자동차 게임의 플레이어 차량 Pawn
//
// 컴포넌트 트리:
//   RootComponent: UBoxComponent (차체 충돌)
//     ├─ UStaticMeshComponent (시각 메시)
//     ├─ USphereComponent x4 (바퀴 — 4코너)
//
// NonScene:
//   ULuaScriptComponent (CarController.lua 등 게임플레이 스크립트)
//
// AGameModeCarGame이 자동 Possess할 첫 APawn 후보 (bAutoPossessPlayer = true).
// ============================================================
class ACarPawn : public APawn
{
public:
	DECLARE_CLASS(ACarPawn, APawn)

	ACarPawn() = default;
	~ACarPawn() override = default;

	// 코드 spawn 시 호출 — 직렬화 경로에선 PostDuplicate가 캐시 포인터를 다시 잡는다.
	void InitDefaultComponents(const FString& StaticMeshFileName = "Data/Truck/TruckBody.obj",
	                           const FString& LuaScriptFile = "CarController.lua");
	void PostDuplicate() override;

	UBoxComponent* GetCollisionBox() const { return CollisionBox; }
	UStaticMeshComponent* GetMesh() const { return Mesh; }
	USphereComponent* GetWheel(int Index) const;
	ULuaScriptComponent* GetLuaScript() const { return LuaScript; }
	UCameraComponent* GetCamera() const { return Camera; }

	// --- Health / Damage ---
	void TakeDamage(float Amount);
	float GetHealth() const { return Health; }

private:
	UBoxComponent* CollisionBox = nullptr;
	UStaticMeshComponent* Mesh = nullptr;
	USphereComponent* Wheels[4] = {};
	ULuaScriptComponent* LuaScript = nullptr;
	UCameraComponent* Camera = nullptr;
	UCarMovementComponent* Movement = nullptr;

	float Health = 100.0f;  // 0 이하 시 사망 처리는 후속 — 현재는 로그만
};
