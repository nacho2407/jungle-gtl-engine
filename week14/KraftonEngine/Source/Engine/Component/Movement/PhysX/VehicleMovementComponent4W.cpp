#include "VehicleMovementComponent4W.h"

#include "GameFramework/AActor.h"
#include "Render/Scene/FScene.h"

namespace
{
	void AddDebugSegment(FScene& Scene, const FVector& A, const FVector& B, const FColor& Color)
	{
		Scene.AddDebugLine(A, B, Color);
	}

	FVector TransformLocalPoint(const FMatrix& LocalToWorld, const FVector& LocalPoint)
	{
		return LocalToWorld.TransformPositionWithW(LocalPoint);
	}

	void AddLocalWireBox(FScene& Scene, const FMatrix& LocalToWorld, const FVector& LocalCenter, const FVector& Extent, const FColor& Color)
	{
		const FVector P[8] =
		{
			LocalCenter + FVector(-Extent.X, -Extent.Y, -Extent.Z),
			LocalCenter + FVector(Extent.X, -Extent.Y, -Extent.Z),
			LocalCenter + FVector(Extent.X,  Extent.Y, -Extent.Z),
			LocalCenter + FVector(-Extent.X,  Extent.Y, -Extent.Z),

			LocalCenter + FVector(-Extent.X, -Extent.Y,  Extent.Z),
			LocalCenter + FVector(Extent.X, -Extent.Y,  Extent.Z),
			LocalCenter + FVector(Extent.X,  Extent.Y,  Extent.Z),
			LocalCenter + FVector(-Extent.X,  Extent.Y,  Extent.Z),
		};

		FVector W[8];
		for (int32 Index = 0; Index < 8; ++Index)
		{
			W[Index] = TransformLocalPoint(LocalToWorld, P[Index]);
		}

		AddDebugSegment(Scene, W[0], W[1], Color);
		AddDebugSegment(Scene, W[1], W[2], Color);
		AddDebugSegment(Scene, W[2], W[3], Color);
		AddDebugSegment(Scene, W[3], W[0], Color);

		AddDebugSegment(Scene, W[4], W[5], Color);
		AddDebugSegment(Scene, W[5], W[6], Color);
		AddDebugSegment(Scene, W[6], W[7], Color);
		AddDebugSegment(Scene, W[7], W[4], Color);

		AddDebugSegment(Scene, W[0], W[4], Color);
		AddDebugSegment(Scene, W[1], W[5], Color);
		AddDebugSegment(Scene, W[2], W[6], Color);
		AddDebugSegment(Scene, W[3], W[7], Color);
	}

	void AddLocalWireCircle(FScene& Scene, const FMatrix& LocalToWorld, const FVector& LocalCenter,
		const FVector& LocalAxisA, const FVector& LocalAxisB, float Radius, int32 Segments, const FColor& Color)
	{
		if (Radius <= 0.0f || Segments < 3)
		{
			return;
		}

		const float Step = 2.0f * FMath::Pi / static_cast<float>(Segments);

		FVector PrevLocal = LocalCenter + LocalAxisA * Radius;
		FVector PrevWorld = TransformLocalPoint(LocalToWorld, PrevLocal);

		for (int32 Index = 1; Index <= Segments; ++Index)
		{
			const float Angle = Step * static_cast<float>(Index);
			const FVector NextLocal = LocalCenter
				+ (LocalAxisA * cosf(Angle) + LocalAxisB * sinf(Angle)) * Radius;

			const FVector NextWorld = TransformLocalPoint(LocalToWorld, NextLocal);
			Scene.AddDebugLine(PrevWorld, NextWorld, Color);
			PrevWorld = NextWorld;
		}
	}

	void AddLocalCross(FScene& Scene, const FMatrix& LocalToWorld, const FVector& LocalCenter, float Size, const FColor& Color)
	{
		AddDebugSegment(Scene,
			TransformLocalPoint(LocalToWorld, LocalCenter - FVector(Size, 0.0f, 0.0f)),
			TransformLocalPoint(LocalToWorld, LocalCenter + FVector(Size, 0.0f, 0.0f)),
			Color);

		AddDebugSegment(Scene,
			TransformLocalPoint(LocalToWorld, LocalCenter - FVector(0.0f, Size, 0.0f)),
			TransformLocalPoint(LocalToWorld, LocalCenter + FVector(0.0f, Size, 0.0f)),
			Color);

		AddDebugSegment(Scene,
			TransformLocalPoint(LocalToWorld, LocalCenter - FVector(0.0f, 0.0f, Size)),
			TransformLocalPoint(LocalToWorld, LocalCenter + FVector(0.0f, 0.0f, Size)),
			Color);
	}

	void AddWheelVisual(FScene& Scene, const FMatrix& LocalToWorld, const FVehicleWheelSetup& Wheel, const FColor& Color)
	{
		const FVector Center = Wheel.Offset;
		const float Radius = Wheel.Radius;
		const float HalfWidth = Wheel.Width * 0.5f;
		constexpr int32 Segments = 24;

		// Wheel side profile: forward/up plane.
		AddLocalWireCircle(Scene, LocalToWorld, Center, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), Radius, Segments, Color);

		// Outer/inner rim hints along local Y.
		AddLocalWireCircle(Scene, LocalToWorld, Center - FVector(0.0f, HalfWidth, 0.0f), FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), Radius, Segments, Color);
		AddLocalWireCircle(Scene, LocalToWorld, Center + FVector(0.0f, HalfWidth, 0.0f), FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), Radius, Segments, Color);

		// Width axis.
		AddDebugSegment(Scene,
			TransformLocalPoint(LocalToWorld, Center - FVector(0.0f, HalfWidth, 0.0f)),
			TransformLocalPoint(LocalToWorld, Center + FVector(0.0f, HalfWidth, 0.0f)),
			Color);

		// Center marker.
		AddLocalCross(Scene, LocalToWorld, Center, std::max(0.08f, Radius * 0.25f), Color);

		// Suspension direction hint. PhysX code uses (0, 0, -1).
		AddDebugSegment(Scene,
			TransformLocalPoint(LocalToWorld, Center),
			TransformLocalPoint(LocalToWorld, Center + FVector(0.0f, 0.0f, -Radius * 1.5f)),
			FColor(180, 180, 180));
	}
}

void UVehicleMovementComponent4W::SetDriveInput(const float Throttle, const float Brake, const float Steer, const bool bReverse) const
{
	if (VehicleInstance)
	{
		VehicleInstance->SetDriveInput(Throttle, Brake, Steer, bReverse);
	}
}

bool UVehicleMovementComponent4W::CreateVehicleInstance(physx::PxPhysics* Physics, physx::PxScene* Scene,
	physx::PxMaterial* Material, const physx::PxTransform& StartPose)
{
	DestroyVehicleInstance();

	VehicleInstance = new FPhysXVehicle4WInstance();
	if (!VehicleInstance->Initialize(Physics, Scene, Material, StartPose, VehicleSetup))
	{
		DestroyVehicleInstance();
		return false;
	}

	return true;
}

void UVehicleMovementComponent4W::DestroyVehicleInstance()
{
	if (VehicleInstance)
	{
		VehicleInstance->Shutdown();
		delete VehicleInstance;
		VehicleInstance = nullptr;
	}
}

physx::PxVehicleWheels* UVehicleMovementComponent4W::GetPxVehicle() const
{
	return VehicleInstance ? VehicleInstance->GetPxVehicle() : nullptr;
}

physx::PxRigidDynamic* UVehicleMovementComponent4W::GetVehicleActor() const
{
	return VehicleInstance ? VehicleInstance->GetActor() : nullptr;
}

void UVehicleMovementComponent4W::ContributeSelectedVisuals(FScene& Scene) const
{
	if (!bShowVehicleShape) return;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	USceneComponent* RootComponent = OwnerActor->GetRootComponent();
	if (!RootComponent)
	{
		return;
	}

	const FMatrix& LocalToWorld = RootComponent->GetWorldMatrix();

	const FColor ChassisColor(80, 200, 255);
	const FColor CenterOfMassColor(255, 220, 80);

	AddLocalWireBox(Scene, LocalToWorld, FVector::ZeroVector, VehicleSetup.ChassisHalfExtent, ChassisColor);
	AddLocalCross(Scene, LocalToWorld, VehicleSetup.CenterOfMassOffset, 0.25f, CenterOfMassColor);

	AddWheelVisual(Scene, LocalToWorld, VehicleSetup.FrontLeftWheel, FColor(255, 80, 80));
	AddWheelVisual(Scene, LocalToWorld, VehicleSetup.FrontRightWheel, FColor(255, 140, 80));
	AddWheelVisual(Scene, LocalToWorld, VehicleSetup.RearLeftWheel, FColor(80, 180, 255));
	AddWheelVisual(Scene, LocalToWorld, VehicleSetup.RearRightWheel, FColor(80, 255, 160));
}
