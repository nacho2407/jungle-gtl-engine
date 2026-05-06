#include "Game/Pawn/CarPawn.h"
#include "Component/BoxComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SphereComponent.h"
#include "Component/LuaScriptComponent.h"
#include "Component/CameraComponent.h"
#include "Component/SpringArmComponent.h"
#include "Game/Component/Movement/CarMovementComponent.h"
#include "Game/Component/CarGasComponent.h"
#include "Game/Component/CarDirtComponent.h"
#include "Game/Component/DirtComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Mesh/ObjManager.h"
#include "Core/CollisionTypes.h"
#include "Core/Log.h"
#include "Math/Rotator.h"
#include "GameFramework/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_CLASS(ACarPawn, APawn)

void ACarPawn::InitDefaultComponents(const FString& StaticMeshFileName, const FString& LuaScriptFile, const FString& LuaCameraScriptFile, const FString& LuaGasScriptFile)
{
	InitChassisComponents(StaticMeshFileName, LuaScriptFile);
	InitPlayerControlledComponents(LuaCameraScriptFile, LuaGasScriptFile);
}

void ACarPawn::InitChassisComponents(const FString& StaticMeshFileName, const FString& LuaScriptFile)
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
	Mesh->SetRelativeLocation(FVector(0.15f, 0.0f, -0.6f));
	Mesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));

	// 3) 바퀴 4개 (Mesh 자식) — 좌표/회전은 Default.Scene 의 시각 TruckTire 메시와 동일 (Mesh 변환 inverse 적용된 값).
	//    Pawn 채널 Block, 시뮬레이션은 Box 가 담당하므로 자체 SimulatePhysics 는 끔.
	const FVector WheelOffsets[4] = {
		FVector(0.95f, -1.4f, 0.1f),
		FVector(0.95f,  1.25f, 0.1f),
		FVector(-0.85f, -1.4f, 0.1f),
		FVector(-0.85f,  1.25f, 0.1f),
	};
	for (int i = 0; i < 4; ++i)
	{
		Wheels[i] = AddComponent<USphereComponent>();
		Wheels[i]->AttachToComponent(Mesh);
		Wheels[i]->SetRelativeLocation(WheelOffsets[i]);
		Wheels[i]->SetSphereRadius(0.4f);
		Wheels[i]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Wheels[i]->SetCollisionObjectType(ECollisionChannel::Pawn);
		Wheels[i]->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
	}

	// 4) 주행 / 운전자 Lua — Player 면 CarController.lua, AI 면 PoliceCarAI.lua 등.
	LuaScript = AddComponent<ULuaScriptComponent>();
	if (!LuaScriptFile.empty())
	{
		LuaScript->SetScriptFile(LuaScriptFile);
	}

	// 5) 차량 물리/이동 컴포넌트 — Lua에서 Throttle/Steering 입력을 받아서 Box에 힘과 토크를 가한다.
	Movement = AddComponent<UCarMovementComponent>();
}

void ACarPawn::InitPlayerControlledComponents(const FString& LuaCameraScriptFile, const FString& LuaGasScriptFile)
{
	// chassis 가 먼저 와야 동작 — InitChassisComponents 가 호출되지 않았다면 skip.
	if (!CollisionBox || !Mesh) return;

	// 1) FirstPersonCamera — 운전석 위치, 차량 회전/이동을 즉시 따라감 (Box 자식).
	FirstPersonCamera = AddComponent<UCameraComponent>();
	FirstPersonCamera->AttachToComponent(CollisionBox);
	FirstPersonCamera->SetRelativeLocation(FVector(0.286f, -0.318f, 0.697f));
	FirstPersonCamera->SetRelativeRotation(FVector(0.0f, 15.0f, 0.0f));

	// 2) ThirdPersonCamera — SpringArm 자식. SpringArm 이 lag 적용해 부드럽게 따라옴.
	USpringArmComponent* SpringArm = AddComponent<USpringArmComponent>();
	SpringArm->AttachToComponent(CollisionBox);
	SpringArm->TargetArmLength = 4.5f;                    // Local -X 방향 거리
	SpringArm->SocketOffset = FVector(0.0f, 0.0f, 2.5f);  // ArmEnd 에서 Z+ 으로 위로
	SpringArm->bEnableCameraLag = true;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraLagSpeed = 6.0f;
	SpringArm->CameraRotationLagSpeed = 8.0f;

	ThirdPersonCamera = AddComponent<UCameraComponent>();
	ThirdPersonCamera->AttachToComponent(SpringArm);
	// 위치/회전은 SpringArm 이 결정 — 카메라 자체는 (0,0,0) 으로 둠.

	// 3) CameraManager.lua — F2 카메라 토글 등 player 카메라 입력 처리.
	LuaCameraScript = AddComponent<ULuaScriptComponent>();
	if (!LuaCameraScriptFile.empty())
	{
		LuaCameraScript->SetScriptFile(LuaCameraScriptFile);
	}

	// 4) 연료 상태 컴포넌트 + GasController.lua — Player 차량의 gas 게이지 / 주유 페이즈 평가에 사용.
	Gas = AddComponent<UCarGasComponent>();

	LuaGasScript = AddComponent<ULuaScriptComponent>();
	if (!LuaGasScriptFile.empty())
	{
		LuaGasScript->SetScriptFile(LuaGasScriptFile);
	}

	// 5) Visual extras — 핸들, 바퀴 시각 메시, 진흙 데칼. 모두 Mesh(차체) 자식.
	//    Map.Scene 에 직접 부착돼있던 컴포넌트들을 코드로 옮긴 것 (placement 시 자동 부착).

	// 5a) 핸들 메시
	UStaticMeshComponent* Handle = AddComponent<UStaticMeshComponent>();
	Handle->AttachToComponent(Mesh);
	if (GEngine)
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		if (UStaticMesh* HandleAsset = FObjManager::LoadObjStaticMesh("Data/Truck/TruckHandle.obj", Device))
			Handle->SetStaticMesh(HandleAsset);
	}

	Handle->SetRelativeLocation(FVector(-0.32f, -0.52f, 0.82f));
	Handle->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	Handle->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 5b) 바퀴 시각 메시 4개 (콜리전은 USphereComponent 가 별도로 담당)
	const FVector TireVisualOffsets[4] = {
		FVector(0.95f, -1.4f, 0.1f),
		FVector(0.95f,  1.25f, 0.1f),
		FVector(-0.85f, -1.4f, 0.1f),
		FVector(-0.85f,  1.25f, 0.1f),
	};
	UStaticMesh* TireAsset = nullptr;
	if (GEngine)
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		TireAsset = FObjManager::LoadObjStaticMesh("Data/Truck/TruckTire.obj", Device);
	}
	for (int i = 0; i < 4; ++i)
	{
		UStaticMeshComponent* Tire = AddComponent<UStaticMeshComponent>();
		Tire->AttachToComponent(Mesh);
		if (TireAsset) Tire->SetStaticMesh(TireAsset);
		FVector TireLocation = TireVisualOffsets[i];
		Tire->SetRelativeLocation(TireLocation);
		Tire->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		Tire->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 5c) 진흙 — UCarDirtComponent (그룹) 자식으로 UDirtComponent (DirtDecal) 1개.
	//      세차 페이즈에서 raycast 로 닦아내는 데칼. Map.Scene 직렬화값과 동일.
	UCarDirtComponent* CarDirt = AddComponent<UCarDirtComponent>();
	CarDirt->AttachToComponent(Mesh);
	CarDirt->SetRelativeLocation(FVector(0.12f, 1.0f, 0.75f));
	CarDirt->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	FTransform DirtTransforms[4] =
	{
		FTransform(FVector(2.25f, -0.02f, -0.1f), FRotator(0.0f, -90.0f, 0.0f), FVector(2.0f, 1.0f, 1.0f)),
		FTransform(FVector(1.12f, 0.0f, 0.17f), FRotator(0.0f, -90.0f, 0.0f), FVector(2.0f, 1.2f, 2.0f)),
		FTransform(FVector(-1.1f, -0.02f, 0.0f), FRotator(0.0f, -90.0f, 0.0f), FVector(2.0f, 1.2f, 2.0f)),
		FTransform(FVector(-0.1f, 0.0f, 0.0f), FRotator(0.0f, -90.0f, 0.0f), FVector(2.0f, 1.2f, 2.0f)),
	};

	for (const FTransform& DirtTransform : DirtTransforms)
	{
		UDirtComponent* Dirt = AddComponent<UDirtComponent>();
		Dirt->AttachToComponent(CarDirt);
		Dirt->SetRelativeTransform(DirtTransform);
	}

	//UDirtComponent* Dirt = AddComponent<UDirtComponent>();
	//Dirt->AttachToComponent(CarDirt);
	//Dirt->SetRelativeLocation(FVector(1.665901f, -0.023956f, -0.602671f));
	//Dirt->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	//Dirt->SetRelativeScale(FVector(2.0f, 1.0f, 1.0f));

	// 6) DirtyCar.lua — 진흙/세차 게임플레이 스크립트.
	ULuaScriptComponent* DirtyCarLua = AddComponent<ULuaScriptComponent>();
	DirtyCarLua->SetScriptFile("DirtyCar.lua");
}

void ACarPawn::BeginPlay()
{
	Super::BeginPlay();
	// Scene-load 경로에서는 PostDuplicate 가 안 도므로 여기서 캐시 포인터 결정.
	// PostDuplicate 경로에선 이미 잡혀 있으니 다시 잡아도 동일 결과 (idempotent).
	ResolveCachedComponents();
}

void ACarPawn::PostDuplicate()
{
	Super::PostDuplicate();
	ResolveCachedComponents();
}

void ACarPawn::ResolveCachedComponents()
{
	CollisionBox = Cast<UBoxComponent>(GetRootComponent());
	Mesh = GetComponentByClass<UStaticMeshComponent>();
	LuaScript = GetComponentByClass<ULuaScriptComponent>();
	Movement = GetComponentByClass<UCarMovementComponent>();
	Gas = GetComponentByClass<UCarGasComponent>();

	// 카메라 두 개를 등록 순서대로 캐싱 — 1) FirstPerson, 2) ThirdPerson.
	FirstPersonCamera = nullptr;
	ThirdPersonCamera = nullptr;
	{
		int CamIdx = 0;
		for (UActorComponent* C : GetComponents())
		{
			if (UCameraComponent* Cam = Cast<UCameraComponent>(C))
			{
				if (CamIdx == 0) FirstPersonCamera = Cam;
				else if (CamIdx == 1) ThirdPersonCamera = Cam;
				++CamIdx;
			}
		}
	}

	// SpringArm 마이그레이션 — 기존 직렬화된 씬은 ThirdPersonCamera 가 CollisionBox
	// 직접 자식. SpringArm 이 없으면 끼워 넣어 lag 효과를 자동 적용. PostDuplicate /
	// BeginPlay 양쪽에서 호출되므로 PIE / scene-load 모두 호환. idempotent.
	if (ThirdPersonCamera && CollisionBox
		&& !Cast<USpringArmComponent>(ThirdPersonCamera->GetParent()))
	{
		USpringArmComponent* SpringArm = AddComponent<USpringArmComponent>();
		SpringArm->AttachToComponent(CollisionBox);
		SpringArm->TargetArmLength = 4.5f;
		SpringArm->SocketOffset = FVector(0.0f, 0.0f, 2.5f);
		SpringArm->bEnableCameraLag = true;
		SpringArm->bEnableCameraRotationLag = true;
		SpringArm->CameraLagSpeed = 6.0f;
		SpringArm->CameraRotationLagSpeed = 8.0f;

		ThirdPersonCamera->AttachToComponent(SpringArm);
		ThirdPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
		ThirdPersonCamera->SetRelativeRotation(FVector(0.0f, 0.0f, 0.0f));
	}

	// Wheels — 컴포넌트 순회 순서대로 캐싱 (InitDefaultComponents 추가 순서 또는 직렬화 순서가 보존된다고 가정)
	for (auto& W : Wheels) W = nullptr;
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

void ACarPawn::TakeMeteorDamage(float Amount)
{
	MeteorHealth -= Amount;
	if (MeteorHealth < 0.0f) MeteorHealth = 0.0f;
	UE_LOG("[Car] Meteor damage %.1f, MeteorHealth=%.1f/%.1f", Amount, MeteorHealth, MaxMeteorHealth);
	// 0 도달 시 GameMode JudgePhaseResult(DodgeMeteor) 가 Failed 로 판정.
}

bool ACarPawn::IsFirstPersonView() const
{
	// E.2/2: PC 경로 — PlayerCameraManager 는 PC 가 보유.
	APlayerController* PC = GetController();
	if (!PC) return false;
	APlayerCameraManager* CamMgr = PC->GetPlayerCameraManager();
	if (!CamMgr) return false;
	return CamMgr->GetActiveCamera() == FirstPersonCamera;
}
