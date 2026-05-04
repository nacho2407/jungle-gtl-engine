#pragma once

#include "GameFramework/AActor.h"
#include "Math/Rotator.h"
#include "Core/Delegate.h"

class UBoxComponent;
class UStaticMeshComponent;
class ULuaScriptComponent;
class ATriggerVolumeBase;
enum class ECarGamePhase : uint8;

// ============================================================
// AWalkingPersonActor — 거리에서 걸어다니는 사람.
//
// 컴포넌트 트리:
//   RootComponent: UBoxComponent (Pawn 채널, SimulatePhysics=true)
//     └─ UStaticMeshComponent (시각 — Person 메시)
//
// NonScene:
//   ULuaScriptComponent ("WalkingPerson.lua") — 30s 주기 회전 + linear velocity 보행
//
// BeginPlay 에서 ATriggerVolumeBase 를 동적 spawn 해 TriggerBox 를 자기 root 에 attach —
// 사람이 걸어 움직이면 트리거 박스도 함께 따라간다. TriggerTag 는 "EscapePolice" 고정.
// 서브-트리거는 EndPlay 에서 destroy.
// ============================================================
class AWalkingPersonActor : public AActor
{
public:
	DECLARE_CLASS(AWalkingPersonActor, AActor)

	AWalkingPersonActor() = default;
	~AWalkingPersonActor() override = default;

	// 코드 spawn 시 호출. 스크립트 / 메시는 인자로 변경 가능.
	void InitDefaultComponents(const FString& StaticMeshFileName = "Data/Map/Person/model_mesh.obj",
	                           const FString& LuaScriptFile = "WalkingPerson.lua");

	void BeginPlay() override;
	void EndPlay() override;
	void PostDuplicate() override;
	void Tick(float DeltaTime) override;

	UBoxComponent*        GetCollisionBox() const { return CollisionBox; }
	UStaticMeshComponent* GetMesh()         const { return Mesh; }
	ULuaScriptComponent*  GetLuaScript()    const { return LuaScript; }
	ATriggerVolumeBase*   GetTrigger()      const { return Trigger; }

private:
	void ResolveCachedComponents();
	void SpawnTrigger();
	void EnsurePhaseListenerBound();
	void HandlePhaseChanged(ECarGamePhase NewPhase);
	void ResetToInitialTransform();

	UBoxComponent*        CollisionBox = nullptr;
	UStaticMeshComponent* Mesh         = nullptr;
	ULuaScriptComponent*  LuaScript    = nullptr;

	// 동적 spawn 된 child trigger — 직렬화 안 됨. EndPlay 에서 정리.
	ATriggerVolumeBase*   Trigger      = nullptr;

	// 트리거 박스 크기 (m). 사람 주변 약 5x5 평면, 높이 3.
	static constexpr float TriggerExtentX = 5.0f;
	static constexpr float TriggerExtentY = 5.0f;
	static constexpr float TriggerExtentZ = 3.0f;

	// Phase 전환마다 돌아갈 시작 위치/회전. BeginPlay 첫 frame 에 캐시.
	FVector  InitialLocation = FVector(0.0f, 0.0f, 0.0f);
	FRotator InitialRotation;
	bool     bInitialCached     = false;
	bool     bPhaseListenerBound = false;
	FDelegateHandle PhaseChangedHandle;
};
