#pragma once

#include "GameFramework/Pawn.h"

class UBoxComponent;
class UStaticMeshComponent;
class USphereComponent;
class ULuaScriptComponent;
class UCameraComponent;
class USpringArmComponent;
class UCarMovementComponent;
class UCarGasComponent;
class UCarDirtComponent;

// ============================================================
// ACarPawn — 자동차 게임의 플레이어 차량 Pawn
//
// 컴포넌트 트리 (placement 시 InitDefaultComponents 가 빌드):
//   RootComponent: UBoxComponent (차체 충돌)            [chassis]
//     ├─ UStaticMeshComponent (TruckBody.obj)           [chassis]
//     │    ├─ UStaticMeshComponent (TruckHandle.obj)    [player]
//     │    ├─ UStaticMeshComponent (TruckTire.obj × 4)  [player]
//     │    └─ UCarDirtComponent → UDirtComponent        [player]
//     ├─ USphereComponent × 4 (콜리전 wheels)            [chassis]
//     ├─ UCameraComponent (FirstPerson)                  [player]
//     └─ USpringArmComponent → UCameraComponent (TPS)    [player]
//
// NonScene:
//   ULuaScriptComponent (CarController / PoliceCarAI 등) [chassis]
//   UCarMovementComponent                                 [chassis]
//   UCarGasComponent                                      [player]
//   ULuaScriptComponent (CameraManager / GasController /  [player]
//     DirtyCar)
//
// 책임 분리: PoliceCar 처럼 AI 차량은 InitChassisComponents 만 호출해서
// Player 전용(Camera/SpringArm/Gas/visual extras) 을 갖지 않는다.
// AGameModeCarGame 이 자동 Possess 할 첫 APawn 후보 (bAutoPossessPlayer = true).
// ============================================================
class ACarPawn : public APawn
{
public:
	DECLARE_CLASS(ACarPawn, APawn)

	ACarPawn() = default;
	~ACarPawn() override = default;

	// Player 차량 셋업 — InitChassisComponents + InitPlayerControlledComponents.
	// 직렬화 / Duplicate 경로엔 InitDefault 를 거치지 않고 BeginPlay/PostDuplicate 의
	// ResolveCachedComponents 가 캐시 포인터를 다시 잡는다.
	void InitDefaultComponents(const FString& StaticMeshFileName = "Data/Truck/TruckBody.obj",
	                           const FString& LuaScriptFile = "CarController.lua",
							   const FString& LuaCameraScriptFile = "CameraManager.lua",
							   const FString& LuaGasScriptFile = "GasController.lua");
	void BeginPlay() override;
	void PostDuplicate() override;

protected:
	// 모든 차량 공통 — Box / Mesh / Wheels(콜리전) / Movement / DriverLua.
	// PoliceCar 등 AI 차량도 이 함수를 호출해 chassis 만 갖춘다.
	virtual void InitChassisComponents(const FString& StaticMeshFileName,
	                                   const FString& LuaScriptFile);

	// Player 차량 전용 — Cameras / SpringArm / Gas / Visual extras (handle/tires/dirt) /
	// 게임플레이 Lua (CameraManager / GasController / DirtyCar). PoliceCar 는 호출 안 함.
	virtual void InitPlayerControlledComponents(const FString& LuaCameraScriptFile,
	                                             const FString& LuaGasScriptFile);

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

	// 명시적 시각 메시 getter — InitPlayerControlled 가 캐시. 직렬화 경로에선
	// ResolveCachedComponents 가 GetComponents() 순회 + 메시 path 매칭 + Box-local 좌표
	// 부호로 4 코너 분류. 인덱스 의존이나 lua 측 좌표 휴리스틱을 피하는 안정적 진입점.
	UStaticMeshComponent* GetHandleMesh()        const { return HandleMesh;        }
	UStaticMeshComponent* GetFrontLeftTireMesh() const { return FrontLeftTireMesh; }
	UStaticMeshComponent* GetFrontRightTireMesh()const { return FrontRightTireMesh;}
	UStaticMeshComponent* GetRearLeftTireMesh()  const { return RearLeftTireMesh;  }
	UStaticMeshComponent* GetRearRightTireMesh() const { return RearRightTireMesh; }

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

	// 시각 메시 캐시 — 인덱스 / 좌표 휴리스틱 의존을 피하기 위해 명시적 멤버로.
	UStaticMeshComponent* HandleMesh        = nullptr;
	UStaticMeshComponent* FrontLeftTireMesh = nullptr;
	UStaticMeshComponent* FrontRightTireMesh= nullptr;
	UStaticMeshComponent* RearLeftTireMesh  = nullptr;
	UStaticMeshComponent* RearRightTireMesh = nullptr;

	static constexpr float MaxMeteorHealth = 50.0f;
	float MeteorHealth = MaxMeteorHealth;   // MeteorPhase 동안만 의미. 운석 충돌마다 차감.
};
