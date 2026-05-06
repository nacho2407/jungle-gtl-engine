#include "Component/SpringArmComponent.h"
#include "Object/ObjectFactory.h"
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
	const FVector ArmEndWorld = LaggedAttachLoc + ArmDirWorld + SocketWorld;

	// (5) World transform 을 *Relative* 로 환산해서 RelativeTransform 에 set —
	//     SceneComponent 기본 합성 (Parent × Relative) 이 우리 의도한 World 를 자식에게 전달.
	const FQuat ParentInvRot = ParentWorldRot.Inverse();
	const FVector RelLoc = ParentInvRot.RotateVector(ArmEndWorld - ParentWorldLoc);
	const FQuat RelRot = (ParentInvRot * LaggedAttachRot).GetNormalized();

	SetRelativeLocation(RelLoc);
	SetRelativeRotation(RelRot);
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
}
