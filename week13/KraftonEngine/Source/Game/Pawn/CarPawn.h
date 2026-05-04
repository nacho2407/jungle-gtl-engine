#pragma once

#include "GameFramework/Pawn.h"

class UBoxComponent;
class UStaticMeshComponent;
class USphereComponent;
class ULuaScriptComponent;
class UCameraComponent;
class UCarMovementComponent;
class UCarGasComponent;

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

	// 코드 spawn 시 호출 — 직렬화 / Duplicate 경로에선 BeginPlay/PostDuplicate 가 캐시 포인터를 다시 잡는다.
	void InitDefaultComponents(const FString& StaticMeshFileName = "Data/Truck/TruckBody.obj",
	                           const FString& LuaScriptFile = "CarController.lua",
							   const FString& LuaCameraScriptFile = "CameraManager.lua",
							   const FString& LuaGasScriptFile = "GasController.lua");
	void BeginPlay() override;
	void PostDuplicate() override;

private:
	// PostDuplicate / BeginPlay 양쪽에서 호출되는 캐시 포인터 재바인딩.
	// PIE 는 PostDuplicate 경로, scene-load 는 BeginPlay 경로 — 둘 다 거쳐야 cached
	// member (Gas / Movement / Wheels 등) 가 nullptr 가 되지 않는다.
	void ResolveCachedComponents();

public:

	UBoxComponent* GetCollisionBox() const { return CollisionBox; }
	UStaticMeshComponent* GetMesh() const { return Mesh; }
	USphereComponent* GetWheel(int Index) const;
	ULuaScriptComponent* GetLuaScript() const { return LuaScript; }
	ULuaScriptComponent* GetLuaCameraScript() const { return LuaCameraScript; }
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	UCameraComponent* GetThirdPersonCamera() const { return ThirdPersonCamera; }
	UCarGasComponent* GetGas() const { return Gas; }

	// --- Meteor Health / Damage ---
	// MeteorPhase 전용 차량 HP. GameState 의 페이즈-실패 카운트와 별개로,
	// 운석 충돌 누적 데미지를 받아 0 이 되면 페이즈 실패. UI 의 메테오 HP 바가 이 값을 폴링.
	void  TakeMeteorDamage(float Amount);
	float GetMeteorHealth() const { return MeteorHealth; }
	float GetMaxMeteorHealth() const { return MaxMeteorHealth; }
	void  SetMeteorHealth(float V) { MeteorHealth = V; }

	bool IsFirstPersonView() const;

private:
	UBoxComponent* CollisionBox = nullptr;
	UStaticMeshComponent* Mesh = nullptr;
	USphereComponent* Wheels[4] = {};
	ULuaScriptComponent* LuaScript = nullptr;
	ULuaScriptComponent* LuaCameraScript = nullptr;
	ULuaScriptComponent* LuaGasScript = nullptr;
	UCameraComponent* FirstPersonCamera = nullptr;
	UCameraComponent* ThirdPersonCamera = nullptr;
	UCarMovementComponent* Movement = nullptr;
	UCarGasComponent* Gas = nullptr;

	static constexpr float MaxMeteorHealth = 50.0f;
	float MeteorHealth = MaxMeteorHealth;   // MeteorPhase 동안만 의미. 운석 충돌마다 차감.
};
