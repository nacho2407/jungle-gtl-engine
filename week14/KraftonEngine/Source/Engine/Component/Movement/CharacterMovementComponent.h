#pragma once

#include "Component/Movement/MovementComponent.h"

#include "Core/Types/CollisionTypes.h"
#include "Core/Types/EngineTypes.h"
#include "Math/Vector.h"
#include "Math/Transform.h"

// UE 의 EMovementMode minimal subset — 후속 단계에서 NavWalking/Swimming 등 확장.
enum class EMovementMode : uint8
{
	Walking,    // floor 위 — 평면 이동 + floor stick, Velocity.Z = 0.
	Falling,    // 공중 — gravity 적용, air control 만.
	WallRunning // 벽 위 — 벽 normal 기반 진행 방향 + 약한 중력 + 벽 부착력.
};

// Walking:
//   input/root-motion XY + small downward probe -> PhysX CCT move.
//   Walking is kept only when a follow-up floor probe confirms a walkable normal.
// Falling:
//   gravity -> PhysX CCT move.
//   downward hit switches back to Walking.
// Collision:
//   movement uses PhysX Character Controller sweep with Pawn-channel query filtering.

#include "Source/Engine/Component/Movement/CharacterMovementComponent.generated.h"

namespace physx
{
	class PxController;
}

struct FControllerMoveResult
{
	bool bHitDown = false;
	bool bHasFloorProbeHit = false;
	bool bHasWalkableFloor = false;
	FHitResult FloorHit;
};

struct FWallRunDebugSnapshot
{
	const char* MovementModeName = "";
	const char* StatusName = "";

	FVector Velocity = FVector::ZeroVector;
	FVector WallNormal = FVector::ZeroVector;
	FVector WallDirection = FVector::ZeroVector;
	FVector HitNormal = FVector::ZeroVector;

	float Speed = 0.0f;
	float PlanarSpeed = 0.0f;
	float AlongWallSpeed = 0.0f;
	float MinStartSpeed = 0.0f;
	float WallCheckDistance = 0.0f;
	float WallCheckSphereRadius = 0.0f;
	float WallRunElapsedTime = 0.0f;
	float MaxWallRunTime = 0.0f;
	float HitDistance = -1.0f;
	float HitUpDot = 0.0f;

	bool bWallRunEnabled = false;
	bool bIsWallRunning = false;
	bool bHasHit = false;
	bool bOnRightSide = false;
	bool bDrawDistanceDebug = false;
	bool bLogDiagnostics = false;
	bool bLegacyScreenText = false;

	FString HitActorName;
	FString HitComponentName;
};

UCLASS()
class UCharacterMovementComponent : public UMovementComponent
{
public:
	GENERATED_BODY()
	UCharacterMovementComponent();
	~UCharacterMovementComponent() override = default;

	void EndPlay() override;

	// Controller 등 외부에서 매 frame 누적. TickComponent 가 ConsumeInputVector 로 비움.
	void AddInputVector(const FVector& WorldDirection, float ScaleValue = 1.0f);

	// Root motion delta 입력 — local 좌표계 (root 본 기준) 의 한 프레임 분.
	// 호출자 (보통 ACharacter::Tick 또는 CMC 가 직접 mesh anim instance 에서) 가 매 frame 누적.
	// 여러 번 호출 시 합성됨 (translation 합산, rotation quat 곱). TickComponent 가 1회 소비.
	// CMC 는 mode 를 모름 — "받으면 적용" 만. 어디서 가져올지는 AnimInstance::RootMotionMode 가 결정.
	void AddRootMotionDelta(const FTransform& LocalDelta);
	bool HasPendingRootMotion() const { return bHasPendingRootMotion; }

	void SetMovementMode(EMovementMode NewMode);
	void Jump();

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	// 직전 TickComponent 에서 root motion 의 yaw 가 capsule rotation 에 실제로 적용됐는지.
	// ACharacter::Tick 이 control yaw 를 capsule 에 덮어쓰기 전에 query 해서 충돌 회피
	// (root motion 회전이 활성 중인 frame 은 control yaw 가 덮으면 회전이 토글되어 끊김).
	bool HasYawDrivenByRootMotion() const { return bAppliedRootMotionYawThisFrame; }

	const FVector& GetVelocity() const { return Velocity; }
	float          GetSpeed()    const { return Velocity.Length(); }
	float          GetMaxWalkSpeed() const { return bWantsSprint ? MaxWalkSpeed * SprintSpeedMultiplier : MaxWalkSpeed; }
	void           SetSprinting(bool bEnable) { bWantsSprint = bEnable; }
	bool           IsSprinting() const { return bWantsSprint; }

	EMovementMode  GetMovementMode() const { return MovementMode; }
	bool           IsWalking() const { return MovementMode == EMovementMode::Walking; }
	bool           IsFalling() const { return MovementMode == EMovementMode::Falling; }
	bool           IsWallRunning() const { return MovementMode == EMovementMode::WallRunning; }
	const char*    GetMovementModeName() const;
	FWallRunDebugSnapshot GetWallRunDebugSnapshot() const;

	// UMovementComponent:
	void Serialize(FArchive& Ar) override;

protected:
	// XY 입력을 velocity 에 반영 + Walking 시 braking. 양 mode 공통 호출.
	void  ApplyInputToVelocity(const FVector& Input, float DeltaTime);

	// Mode 별 Z 처리 + 위치 갱신.
	// RootMotionWorldXY 는 이번 frame 의 root motion 평면 변위 (world frame, Z=0 보장).
	// XY 적용 단계에 합산되고 floor stick / gravity 는 mode 가 자체 결정.
	void  TickWalking(float DeltaTime, const FVector& RootMotionWorldXY);
	void  TickFalling(float DeltaTime, const FVector& RootMotionWorldXY);
	void  TickWallRunning(float DeltaTime, const FVector& RootMotionWorldXY, const FVector& Input);
	bool  FindFloor(FHitResult& OutFloorHit) const;
	bool  IsWalkableFloorHit(const FHitResult& Hit) const;

	// 벽타기 진입/유지에 필요한 벽 후보 검사.
	bool  FindWallRunSurface(FHitResult& OutHit, bool& bOutRightSide) const;
	void  BuildWallRunSweepCandidates(bool bRightSide, TArray<FVector>& OutCandidates) const;
	bool  SweepWallRunSide(bool bRightSide, FHitResult& OutHit) const;
	bool  SweepWallRunDirection(const FVector& Direction, FHitResult& OutHit) const;
	bool  SweepWallRunStaticMeshes(const FVector& Start, const FVector& Direction, FHitResult& OutHit) const;
	bool  SweepWallRunStaticMeshBounds(const FVector& Start, const FVector& Direction, FHitResult& OutHit) const;
	bool  IsRunnableWall(const FVector& WallNormal) const;
	bool  TryStartWallRun();
	void  StartWallRun(const FHitResult& WallHit, bool bRightSide);
	void  EndWallRun();
	void  PerformWallJump();
	FVector ComputeWallRunDirection(const FVector& WallNormal) const;

	enum class EWallRunStatus : uint8
	{
		NotFalling,
		Disabled,
		NoUpdatedComponent,
		NoController,
		NoWall,
		LowSpeed,
		BadNormal,
		BadDirection,
		Active,
		EndedTimeLimit,
		EndedNoWall,
		EndedBadNormal,
		EndedBadDirection,
		Landed
	};

	void        SetWallRunStatus(EWallRunStatus NewStatus, const FHitResult* Hit = nullptr);
	const char* GetWallRunStatusName(EWallRunStatus Status) const;
	void        DrawWallRunStatusText() const;
	void        DrawWallRunDistanceDebug() const;
	bool        ShouldEmitWallRunDiagnostics() const;
	void        LogWallRunStatus(EWallRunStatus Status, const FHitResult* Hit) const;
	float       GetWallRunCapsuleRadius() const;
	FVector     GetWallRunSweepStart(const FVector& Direction) const;
	float       GetWallRunAlongSpeed(const FVector& WallNormal) const;
	float       GetWallRunInputAlong(const FVector& Input, const FVector& RunDirection) const;

	FVector       AccumulatedInput = FVector(0.0f, 0.0f, 0.0f);
	FVector       Velocity         = FVector(0.0f, 0.0f, 0.0f);
	// 시작 시 floor 잡힐 때까지 Falling — 첫 frame TickFalling 이 raycast 후 자동 Walking 전환.
	EMovementMode MovementMode     = EMovementMode::Falling;

	// 현재 벽타기 대상 벽 정보 — 벽 점프/이동/종료 판정이 같은 벽 기준으로 계산되도록 보존.
	FVector       WallRunNormal     = FVector::ZeroVector;
	FVector       WallRunDirection  = FVector::ZeroVector;
	float         WallRunElapsedTime = 0.0f;
	bool          bWallRunOnRightSide = false;

	// Wall-jump 직후 같은 벽에 즉시 재부착되는 핑퐁을 막기 위한 상태.
	// Timer 가 양수인 동안 normal 이 LastWallJumpNormal 과 가까운 벽 후보는 TryStartWallRun 에서 거절.
	FVector       LastWallJumpNormal = FVector::ZeroVector;
	float         WallJumpReattachTimer = 0.0f;

	EWallRunStatus LastWallRunStatus = EWallRunStatus::NotFalling;
	FHitResult     LastWallRunStatusHit;
	bool           bLastWallRunStatusHasHit = false;
	mutable int32  WallRunDiagnosticsFrameCounter = 0;

	// Jump() 가 set, TickWalking/TickFalling 이 consume. edge-triggered 라 동일 프레임 다중 호출도 1회 점프.
	bool          bWantsJump       = false;

	// 남은 점프 횟수. 착지 시 MaxJumpCount 로 reset, 점프 1회 사용 시 1 감소.
	// 첫 frame 은 Falling 으로 시작하므로 0 — 초기 낙하 중 공짜 점프 방지.
	int32         JumpsRemaining   = 0;

	int32 GroundMissFrames = 0;

	// Root motion 누적 buffer — 매 frame AddRootMotionDelta 로 합성, TickComponent 가 1회 소비.
	// PendingRootMotion 이 identity 라도 "이번 frame 에 root motion 이 있었다" 와 구분 필요해 bool 별도.
	FTransform    PendingRootMotion;
	bool          bHasPendingRootMotion = false;

	// 직전 TickComponent 에서 root motion yaw 가 실제 적용됐는지 (외부 query 용 — Character 의 yaw 가드).
	// 매 Tick 시작에 reset 후 yaw 적용 시 true.
	bool          bAppliedRootMotionYawThisFrame = false;

	// 평면 속도 기준 yaw 를 RotationYawRate * dt 로 lerp. TickComponent 끝에서 적용.
	void  PhysOrientToMovement(float DeltaTime);

private:
	bool EnsureController();
	void ReleaseController();

	void SyncUpdatedComponentFromController();
	void SyncControllerToUpdatedComponentIfNeeded();

	FControllerMoveResult MoveController(const FVector& Delta, float DeltaTime);
	
	void ConsumeInputVector(FVector& OutAccumulated);
	bool ConsumePendingRootMotion(FTransform& OutLocalDelta);

public:
	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Max Walk Speed", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float MaxWalkSpeed = 8.0f;     // m/s — 기존 6.0f 기준 1.5배
	float SprintSpeedMultiplier = 1.8f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Max Acceleration", Min = 0.0f, Max = 200.0f, Speed = 0.5f)
	float MaxAcceleration = 20.0f;    // m/s^2
	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Braking Friction", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float BrakingFriction = 8.0f;     // 입력 없을 때 감속률 (m/s^2). Walking 만 적용.
	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Gravity", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float Gravity = 9.8f;     // m/s^2 (positive — 적용 시 Velocity.Z -= Gravity*dt)
	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Floor Probe Distance", Min = 0.0f, Max = 5.0f, Speed = 0.01f)
	float FloorProbeDistance = 0.1f;     // capsule HalfHeight 아래 추가 probe 거리
	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Jump Z Velocity", Min = 0.0f, Max = 50.0f, Speed = 0.1f)
	float JumpZVelocity = 7.5f;     // m/s — Jump 시 Velocity.Z 에 박는 값. (~2.87m apex with g=9.8)
	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Max Jump Count", Min = 1, Max = 5, Speed = 1)
	int32 MaxJumpCount = 2;         // 지상 점프 1회 + 공중 점프 (MaxJumpCount - 1) 회.
	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Max Falling Slide Speed", Min = 0.0f, Max = 50.0f, Speed = 0.1f)
	float MaxFallingSlideSpeed = 4.0f;  // m/s — Falling 중 걸을 수 없는 슬로프에 닿아 있을 때 Z 속도 누적 상한.

	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun|WallJump", DisplayName = "Wall Jump Out Velocity", Min = 0.0f, Max = 50.0f, Speed = 0.1f)
	float WallJumpOutVelocity = 8.0f;     // m/s — 벽 normal 방향 푸시.
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun|WallJump", DisplayName = "Wall Jump Up Velocity", Min = 0.0f, Max = 50.0f, Speed = 0.1f)
	float WallJumpUpVelocity = 6.0f;      // m/s — 위쪽 임펄스.
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun|WallJump", DisplayName = "Wall Jump Forward Velocity", Min = 0.0f, Max = 50.0f, Speed = 0.1f)
	float WallJumpForwardVelocity = 3.0f; // m/s — 벽 진행 방향 보너스 (모멘텀 보존).
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun|WallJump", DisplayName = "Wall Jump Reattach Cooldown", Min = 0.0f, Max = 2.0f, Speed = 0.01f)
	float WallJumpReattachCooldown = 0.25f; // s — 같은 벽 즉시 재진입 방지.
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun|WallJump", DisplayName = "Wall Jump Reattach Normal Dot", Min = 0.0f, Max = 1.0f, Speed = 0.01f)
	float WallJumpReattachNormalDot = 0.9f; // 마지막 wall-jump normal 과 |dot| 이 이 값 이상이면 "같은 벽" 으로 보고 쿨다운 동안 거절.

	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Orient Rotation To Movement")
	bool  bOrientRotationToMovement = true;
	UPROPERTY(Edit, Save, Category = "CharacterMovement", DisplayName = "Rotation Yaw Rate", Min = 0.0f, Max = 3600.0f, Speed = 5.0f)
	float RotationYawRate = 540.0f;   // deg/sec

	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun", DisplayName = "Enable Wall Run")
	bool bEnableWallRun = true;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun", DisplayName = "Wall Check Distance", Min = 0.0f, Max = 5.0f, Speed = 0.05f)
	float WallCheckDistance = 0.35f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun", DisplayName = "Wall Check Sphere Radius", Min = 0.01f, Max = 1.0f, Speed = 0.01f)
	float WallCheckSphereRadius = 0.12f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun", DisplayName = "Runnable Wall Up Dot", Min = 0.0f, Max = 1.0f, Speed = 0.01f)
	float RunnableWallUpDot = 0.2f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun", DisplayName = "Min Wall Run Start Speed", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float MinWallRunStartSpeed = 3.0f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun", DisplayName = "Wall Run Min Speed", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float WallRunMinSpeed = 9.0f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun", DisplayName = "Wall Run Max Speed", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float WallRunMaxSpeed = 18.0f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun", DisplayName = "Wall Run Acceleration", Min = 0.0f, Max = 200.0f, Speed = 0.5f)
	float WallRunAcceleration = 24.0f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun", DisplayName = "Wall Run Gravity Scale", Min = 0.0f, Max = 1.0f, Speed = 0.01f)
	float WallRunGravityScale = 0.25f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun", DisplayName = "Max Wall Run Slide Speed", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float MaxWallRunSlideSpeed = 2.0f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun", DisplayName = "Wall Stick Acceleration", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float WallStickAcceleration = 4.0f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun", DisplayName = "Max Wall Run Time", Min = 0.0f, Max = 10.0f, Speed = 0.1f)
	float MaxWallRunTime = 1.5f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun|Diagnostics", DisplayName = "Legacy Screen Status Text")
	bool bShowWallRunStatusText = false;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun|Diagnostics", DisplayName = "Log Wall Run Diagnostics")
	bool bLogWallRunDiagnostics = true;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun|Diagnostics", DisplayName = "Wall Run Diagnostics Interval", Min = 1, Max = 120, Speed = 1)
	int32 WallRunDiagnosticsInterval = 15;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|WallRun|Diagnostics", DisplayName = "Draw Wall Run Distance")
	bool bDrawWallRunDistanceDebug = true;

private:
	physx::PxController* Controller = nullptr;

	UPROPERTY(Edit, Save, Category = "CharacterMovement|Controller", DisplayName = "Controller Contact Offset", Min = 0.001f, Max = 1.0f, Speed = 0.01f)
	float ControllerContactOffset = 0.05f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|Controller", DisplayName = "Max Step Height", Min = 0.0f, Max = 5.0f, Speed = 0.01f)
	float MaxStepHeight = 0.4f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|Controller", DisplayName = "Walkable Slope Angle", Min = 0.0f, Max = 89.0f, Speed = 1.0f)
	float WalkableSlopeAngle = 45.0f;
	UPROPERTY(Edit, Save, Category = "CharacterMovement|Controller", DisplayName = "Controller Min Move Distance", Min = 0.0f, Max = 0.1f, Speed = 0.001f)
	float ControllerMinMoveDistance = 0.001f;

	UPROPERTY(Edit, Save, Category = "CharacterMovement|Controller", DisplayName = "Ground Miss Tolerance Frames", Min = 0, Max = 10, Speed = 1)
	int32 GroundMissToleranceFrames = 2;

	UPROPERTY(Edit, Save, Category = "CharacterMovement|Controller", DisplayName = "Controller Sync Teleport Distance", Min = 0.0f, Max = 100.0f, Speed = 0.1f)
	float ControllerSyncTeleportDistance = 1.0f;

	bool bWantsSprint = false;
	USceneComponent* ControllerUpdatedComponent = nullptr;
	float CachedControllerRadius = 0.0f;
	float CachedControllerHalfHeight = 0.0f;
	float CachedControllerContactOffset = 0.0f;
	float CachedMaxStepHeight = 0.0f;
	float CachedWalkableSlopeAngle = 0.0f;
};
