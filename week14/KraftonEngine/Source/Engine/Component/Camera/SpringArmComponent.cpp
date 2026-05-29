#include "Component/Camera/SpringArmComponent.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Core/Types/CollisionTypes.h"
#include "Math/Rotator.h"
#include <algorithm>
#include <cmath>

void USpringArmComponent::BeginPlay()
{
	Super::BeginPlay();
	bHasPreviousState = false;
}

void USpringArmComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// SpringArm 은 부모가 있어야 의미가 있음. 부모 없으면 spring 동작은 skip 하고
	// SceneComponent 기본 transform 합성에 맡긴다.
	if (!ParentComponent)
	{
		return;
	}

	// (1) 부모 World transform 추출. 두 개 분리:
	//   - ParentActualRot: capsule 의 실제 world rotation (RelativeRotation 환산용 — 불변).
	//   - DesiredParentRot: SpringArm 이 사용할 desired rotation (control rotation 적용 후).
	const FMatrix& ParentWorld = ParentComponent->GetWorldMatrix();
	const FVector ParentWorldLoc = ParentComponent->GetWorldLocation();
	const FQuat   ParentActualRot  = ParentWorld.ToQuat().GetNormalized();
	FQuat         DesiredParentRot = ParentActualRot;

	// bUsePawnControlRotation — capsule rotation 대신 owner APawn 의 ControlRotation 을
	// (선택된 axis 별로) 사용. mouse look 이 capsule 안 건드리고 카메라만 회전하는 패턴.
	if (bUsePawnControlRotation)
	{
		if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			FRotator Result = DesiredParentRot.ToRotator();
			const FRotator Ctrl = OwnerPawn->GetControlRotation();
			if (bInheritPitch) Result.Pitch = Ctrl.Pitch;
			if (bInheritYaw)   Result.Yaw   = Ctrl.Yaw;
			if (bInheritRoll)  Result.Roll  = Ctrl.Roll;
			DesiredParentRot = Result.ToQuaternion();
		}
	}

	// (2) Desired attach point — 부모 위치 + desired 회전 기준 TargetOffset 적용.
	const FVector DesiredAttachLoc = ParentWorldLoc + DesiredParentRot.RotateVector(TargetOffset);
	const FQuat DesiredAttachRot = DesiredParentRot;

	// (3) Lag 적용 — 첫 Tick 은 desired 로 초기화 (아직 비교할 prev 없음).
	if (!bHasPreviousState)
	{
		LaggedAttachRot = DesiredAttachRot;
		LaggedAttachLoc = DesiredAttachLoc;
		bHasPreviousState = true;
	}
	else
	{
		if (bEnableCameraRotationLag && CameraRotationLagSpeed > 0.0f)
		{
			const float Alpha = std::min(DeltaTime * CameraRotationLagSpeed, 1.0f);
			LaggedAttachRot = FQuat::Slerp(LaggedAttachRot, DesiredAttachRot, Alpha).GetNormalized();
		}
		else
		{
			LaggedAttachRot = DesiredAttachRot;
		}

		if (bEnableCameraLag && CameraLagSpeed > 0.0f)
		{
			const float Alpha = std::min(DeltaTime * CameraLagSpeed, 1.0f);
			FVector NewLoc = LaggedAttachLoc + (DesiredAttachLoc - LaggedAttachLoc) * Alpha;

			// 너무 멀어지면 클램프 — 빠른 텔레포트/리스폰 직후 카메라가 한참 뒤따라오는 현상 방지.
			if (CameraLagMaxDistance > 0.0f)
			{
				const float DistSq = FVector::DistSquared(DesiredAttachLoc, NewLoc);
				const float MaxSq = CameraLagMaxDistance * CameraLagMaxDistance;
				if (DistSq > MaxSq)
				{
					const FVector Diff = DesiredAttachLoc - NewLoc;
					NewLoc = DesiredAttachLoc - Diff.Normalized() * CameraLagMaxDistance;
				}
			}
			LaggedAttachLoc = NewLoc;
		}
		else
		{
			LaggedAttachLoc = DesiredAttachLoc;
		}
	}

	// (4) ArmEnd 계산 — SpringArm 의 World 위치 (자식 카메라가 여기 부착됨).
	//     LaggedAttach 에서 Local -X 방향으로 TargetArmLength 만큼 + SocketOffset.
	const FVector ArmDirWorld = LaggedAttachRot.RotateVector(FVector(-TargetArmLength, 0.0f, 0.0f));
	const FVector SocketWorld = LaggedAttachRot.RotateVector(SocketOffset);
	FVector ArmEndWorld = LaggedAttachLoc + ArmDirWorld + SocketWorld;

	// (4b) Collision test — bDoCollisionTest 가 켜져 있으면 LaggedAttach → ArmEnd 방향으로
	//      raycast. Hit 이 있으면 해당 거리에서 ProbeSize 만큼 안쪽에서 정지해 카메라가
	//      벽 너머로 빠지지 않게 한다. 자기 Owner 액터는 ignore. (UE 의 sphere sweep 은 본
	//      엔진 미지원이라 단일 ray + ProbeSize 안전 거리로 근사.)
	if (bDoCollisionTest)
	{
		AActor* Owner = GetOwner();
		UWorld* World = Owner ? Owner->GetWorld() : nullptr;
		if (World)
		{
			const FVector Diff = ArmEndWorld - LaggedAttachLoc;
			const float Distance = Diff.Length();
			if (Distance > 1e-4f)
			{
				const FVector Dir = Diff / Distance;
				FHitResult Hit;
				if (World->PhysicsSweepSphere(LaggedAttachLoc, Dir, Distance, ProbeSize, Hit, ProbeChannel, Owner))
				{
					const float SafeDist = std::max(Hit.Distance, 0.0f);
					ArmEndWorld = LaggedAttachLoc + Dir * SafeDist;
				}
			}
		}
	}

	// (5) World transform 을 *Relative* 로 환산해서 RelativeTransform 에 set —
	//     SceneComponent 기본 합성 (Parent 실제 × Relative) 이 우리 의도한 World 를 자식에게 전달.
	//     ★ 반드시 ParentActualRot 의 inverse 사용 (DesiredParentRot 아님). 안 그러면
	//       (Desired)^-1 × Lagged 가 Desired ≈ Lagged 일 때 identity 되어 카메라 가 capsule
	//       회전만 따라감 — control rotation 이 무시되는 버그.
	const FQuat ParentInvRot = ParentActualRot.Inverse();
	const FVector RelLoc = ParentInvRot.RotateVector(ArmEndWorld - ParentWorldLoc);
	const FQuat RelRot = (ParentInvRot * LaggedAttachRot).GetNormalized();

	SetRelativeLocation(RelLoc);
	SetRelativeRotation(RelRot);
}
