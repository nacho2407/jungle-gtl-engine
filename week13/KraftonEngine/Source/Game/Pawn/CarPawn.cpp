#include "Game/Pawn/CarPawn.h"
#include "Component/BoxComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SphereComponent.h"
#include "Component/LuaScriptComponent.h"
#include "Component/CameraComponent.h"
#include "Game/Component/Movement/CarMovementComponent.h"
#include "Game/Component/CarGasComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Mesh/ObjManager.h"
#include "Core/CollisionTypes.h"
#include "Core/Log.h"
#include "Math/Rotator.h"
#include "GameFramework/CameraManager.h"

IMPLEMENT_CLASS(ACarPawn, APawn)

void ACarPawn::InitDefaultComponents(const FString& StaticMeshFileName, const FString& LuaScriptFile, const FString& LuaCameraScriptFile)
{
	// 1) Root = 차체 Box (충돌만 — 시뮬레이션은 끄고 Lua가 트랜스폼 직접 조작)
	// SimulatePhysics=true로 두면 NativePhysicsScene의 중력/속도 적분과 Lua의
	// 트랜스폼 setter가 매 Tick 충돌하므로, MVP에선 false. 추후 AddForce 기반으로
	// Lua를 재작성하면 다시 켤 수 있다.
	CollisionBox = AddComponent<UBoxComponent>();
	SetRootComponent(CollisionBox);
	CollisionBox->SetBoxExtent(FVector(2.0f, 1.0f, 0.5f));
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionObjectType(ECollisionChannel::Pawn);
	CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
	CollisionBox->SetSimulatePhysics(true);

	// 차량 1.5톤, mass center를 차체 아래(-Z) 30cm로 — 회전 안정성 향상.
	// 코너링/경사로에서 차량이 쉽게 뒤집히지 않도록 무게중심을 낮춘다.
	CollisionBox->SetMass(1500.0f);
	CollisionBox->SetCenterOfMass(FVector(0.0f, 0.0f, -0.3f));

	// 2) 차체 메시 (Box 자식 — 시각만, 충돌은 Box가 담당)
	Mesh = AddComponent<UStaticMeshComponent>();
	Mesh->AttachToComponent(CollisionBox);
	if (GEngine)
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		if (UStaticMesh* Asset = FObjManager::LoadObjStaticMesh(StaticMeshFileName, Device))
			Mesh->SetStaticMesh(Asset);
	}
	Mesh->SetRelativeLocation(FVector(0.155f, 0.0f, -0.607f));
	Mesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 00.0f));

	// 3) 바퀴 4개 (Box 자식, 4코너) — Pawn 채널 Block, 시뮬레이션은 Box가 담당하므로 자체 SimulatePhysics는 끔
	const FVector WheelOffsets[4] = {
		FVector( 1.5f,  0.8f, -0.5f),  // 전 우
		FVector( 1.5f, -0.8f, -0.5f),  // 전 좌
		FVector(-1.5f,  0.8f, -0.5f),  // 후 우
		FVector(-1.5f, -0.8f, -0.5f),  // 후 좌
	};
	for (int i = 0; i < 4; ++i)
	{
		Wheels[i] = AddComponent<USphereComponent>();
		Wheels[i]->AttachToComponent(CollisionBox);
		Wheels[i]->SetRelativeLocation(WheelOffsets[i]);
		Wheels[i]->SetSphereRadius(0.4f);
		Wheels[i]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Wheels[i]->SetCollisionObjectType(ECollisionChannel::Pawn);
		Wheels[i]->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
	}

	// 4) 1/3인칭 카메라 (Box 자식 — 차량 회전/이동에 자동 따라감)
	// APawn::PossessedBy에서 자동으로 ActiveCamera로 설정된다.
	FirstPersonCamera = AddComponent<UCameraComponent>();
	FirstPersonCamera->AttachToComponent(CollisionBox);
	FirstPersonCamera->SetRelativeLocation(FVector(0.286f, -0.318f, 0.697f));
	FirstPersonCamera->SetRelativeRotation(FVector(0.0f, 15.0f, 0.0f));

	ThirdPersonCamera = AddComponent<UCameraComponent>();
	ThirdPersonCamera->AttachToComponent(CollisionBox);
	ThirdPersonCamera->SetRelativeLocation(FVector(-4.5f, 0.0f, 2.5f));

	// 5) Lua 스크립트 — Tick에서 입력 읽고 차량 제어
	LuaScript = AddComponent<ULuaScriptComponent>();
	if (!LuaScriptFile.empty())
	{
		LuaScript->SetScriptFile(LuaScriptFile);
	}

	LuaCameraScript = AddComponent<ULuaScriptComponent>();
	if (!LuaCameraScriptFile.empty())
	{
		LuaCameraScript->SetScriptFile(LuaCameraScriptFile);
	}

	// 6) 차량 물리/이동 컴포넌트 — Lua에서 Throttle/Steering 입력을 받아서 Box에 힘과 토크를 가한다.
	Movement = AddComponent<UCarMovementComponent>();

	// 7) 연료 상태 컴포넌트 — 이동 정책과 분리해서 자동차의 gas 보유량만 관리한다.
	Gas = AddComponent<UCarGasComponent>();
}

void ACarPawn::PostDuplicate()
{
	Super::PostDuplicate();

	CollisionBox = Cast<UBoxComponent>(GetRootComponent());
	Mesh = GetComponentByClass<UStaticMeshComponent>();
	LuaScript = GetComponentByClass<ULuaScriptComponent>();
	FirstPersonCamera = GetComponentByClass<UCameraComponent>();
	Movement = GetComponentByClass<UCarMovementComponent>();
	Gas = GetComponentByClass<UCarGasComponent>();

	// Wheels — 컴포넌트 순회 순서대로 캐싱 (InitDefaultComponents 추가 순서 또는 직렬화 순서가 보존된다고 가정)
	int Idx = 0;
	for (UActorComponent* C : GetComponents())
	{
		if (USphereComponent* S = Cast<USphereComponent>(C))
		{
			if (Idx < 4) Wheels[Idx++] = S;
		}
	}
}

USphereComponent* ACarPawn::GetWheel(int Index) const
{
	return (Index >= 0 && Index < 4) ? Wheels[Index] : nullptr;
}

void ACarPawn::TakeDamage(float Amount)
{
	Health -= Amount;
	UE_LOG("[Car] Damage %.1f, Health=%.1f", Amount, Health);
	// 사망 처리(페이즈 종료, 게임오버 UI 등)는 후속 작업
}

bool ACarPawn::IsFirstPersonView() const
{
	UCameraManager* CamMgr = GetWorld()->GetCameraManager();
	if (!CamMgr) return false;
	return CamMgr->GetActiveCamera() == FirstPersonCamera;
}
