#include "Component/SpringArmComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Core/CollisionTypes.h"
#include <algorithm>
#include <cmath>

IMPLEMENT_CLASS(USpringArmComponent, USceneComponent)

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

	// (1) 부모 World transform 추출. Pawn 의 위치/회전이 desired attach point.
	const FMatrix& ParentWorld = ParentComponent->GetWorldMatrix();
	const FVector ParentWorldLoc = ParentComponent->GetWorldLocation();
	const FQuat ParentWorldRot = ParentWorld.ToQuat().GetNormalized();

	// (2) Desired attach point — 부모 위치 + 부모 회전 기준 TargetOffset 적용.
	const FVector DesiredAttachLoc = ParentWorldLoc + ParentWorldRot.RotateVector(TargetOffset);
	const FQuat DesiredAttachRot = ParentWorldRot;

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
				if (World->PhysicsRaycast(LaggedAttachLoc, Dir, Distance, Hit, ProbeChannel, Owner))
				{
					const float SafeDist = std::max(Hit.Distance - ProbeSize, 0.0f);
					ArmEndWorld = LaggedAttachLoc + Dir * SafeDist;
				}
			}
		}
	}

	// (5) World transform 을 *Relative* 로 환산해서 RelativeTransform 에 set —
	//     SceneComponent 기본 합성 (Parent × Relative) 이 우리 의도한 World 를 자식에게 전달.
	const FQuat ParentInvRot = ParentWorldRot.Inverse();
	const FVector RelLoc = ParentInvRot.RotateVector(ArmEndWorld - ParentWorldLoc);
	const FQuat RelRot = (ParentInvRot * LaggedAttachRot).GetNormalized();

	SetRelativeLocation(RelLoc);
	SetRelativeRotation(RelRot);
}

void USpringArmComponent::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);

	// 튜닝 파라미터만 직렬화. LaggedAttachLoc / LaggedAttachRot / bHasPreviousState 는
	// 매 BeginPlay 시 첫 Tick 에서 부모 World 로 다시 초기화되는 런타임 상태라 제외.
	Ar << TargetArmLength;
	Ar << SocketOffset;
	Ar << TargetOffset;
	Ar << bEnableCameraLag;
	Ar << bEnableCameraRotationLag;
	Ar << CameraLagSpeed;
	Ar << CameraRotationLagSpeed;
	Ar << CameraLagMaxDistance;
	Ar << bDoCollisionTest;
	Ar << ProbeSize;
	// ProbeChannel 은 enum class — Archive operator 가 enum 도 받지만, 안전하게 underlying 으로 직렬화.
	uint8 ProbeChannelByte = static_cast<uint8>(ProbeChannel);
	Ar << ProbeChannelByte;
	if (Ar.IsLoading())
	{
		ProbeChannel = static_cast<ECollisionChannel>(ProbeChannelByte);
	}
}

void USpringArmComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Target Arm Length",        EPropertyType::Float, "SpringArm", &TargetArmLength,           0.0f, 100000.0f, 1.0f });
	OutProps.push_back({ "Socket Offset",            EPropertyType::Vec3,  "SpringArm", &SocketOffset,              0.0f, 0.0f,      0.1f });
	OutProps.push_back({ "Target Offset",            EPropertyType::Vec3,  "SpringArm", &TargetOffset,              0.0f, 0.0f,      0.1f });
	OutProps.push_back({ "Enable Camera Lag",        EPropertyType::Bool,  "SpringArm", &bEnableCameraLag                                });
	OutProps.push_back({ "Enable Rotation Lag",      EPropertyType::Bool,  "SpringArm", &bEnableCameraRotationLag                        });
	OutProps.push_back({ "Camera Lag Speed",         EPropertyType::Float, "SpringArm", &CameraLagSpeed,            0.0f, 1000.0f,   0.1f });
	OutProps.push_back({ "Camera Rotation Lag Speed",EPropertyType::Float, "SpringArm", &CameraRotationLagSpeed,    0.0f, 1000.0f,   0.1f });
	OutProps.push_back({ "Camera Lag Max Distance",  EPropertyType::Float, "SpringArm", &CameraLagMaxDistance,      0.0f, 100000.0f, 1.0f });
	OutProps.push_back({ "Do Collision Test",        EPropertyType::Bool,  "SpringArm", &bDoCollisionTest                                });
	OutProps.push_back({ "Probe Size",               EPropertyType::Float, "SpringArm", &ProbeSize,                 0.0f, 100.0f,    0.01f });
}
