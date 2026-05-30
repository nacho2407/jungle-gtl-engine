#include "BodyInstance.h"

#include "PhysXTypeConversions.h"
#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/Shape/SphereComponent.h"
#include <algorithm>

physx::PxRigidDynamic* FBodyInstance::GetRigidDynamic() const
{
	return RigidActor ? RigidActor->is<physx::PxRigidDynamic>() : nullptr;
}

physx::PxRigidStatic* FBodyInstance::GetRigidStatic() const
{
	return RigidActor ? RigidActor->is<physx::PxRigidStatic>() : nullptr;
}

FTransform FBodyInstance::GetBodyTransform() const
{
	if (!RigidActor)
	{
		return FTransform();
	}

	const physx::PxTransform Pose = RigidActor->getGlobalPose();
	return PhysXConvert::ToFTransform(Pose);
}

void FBodyInstance::SetBodyTransform(const FTransform& WorldTransform)
{
	if (!RigidActor) return;

	RigidActor->setGlobalPose(PhysXConvert::ToPxTransform(WorldTransform));
}

void FBodyInstance::SetKinematicTarget(const FTransform& WorldTransform)
{
	physx::PxRigidDynamic* Dynamic = GetRigidDynamic();
	if (!Dynamic)
	{
		SetBodyTransform(WorldTransform);
		return;
	}

	if (!(Dynamic->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC))
	{
		Dynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
	}

	Dynamic->setKinematicTarget(PhysXConvert::ToPxTransform(WorldTransform));
}

// 지속적인 힘(dt영향)
void FBodyInstance::AddForce(const FVector& Force)
{
	physx::PxRigidDynamic* Dynamic = GetRigidDynamic();
	if (!Dynamic) return;

	Dynamic->addForce(PhysXConvert::ToPxVec3(Force), physx::PxForceMode::eFORCE);
}

// 순간 충력
void FBodyInstance::AddImpulse(const FVector& Impulse)
{
	physx::PxRigidDynamic* Dynamic = GetRigidDynamic();
	if (!Dynamic) return;

	Dynamic->addForce(PhysXConvert::ToPxVec3(Impulse), physx::PxForceMode::eIMPULSE);
}

// PhysX dynamic actor는 가만히 있으면 sleep상태가 될 수 있다함 -> 깨워줘야함
void FBodyInstance::WakeUp()
{
	physx::PxRigidDynamic* Dynamic = GetRigidDynamic();
	if (!Dynamic) return;

	Dynamic->wakeUp();
}

void FBodyInstance::SetLinearVelocity(const FVector& Velocity) 
{
	physx::PxRigidDynamic* Dynamic = GetRigidDynamic();
	if (!Dynamic) return;

	Dynamic->setLinearVelocity(PhysXConvert::ToPxVec3(Velocity));
}

FVector FBodyInstance::GetLinearVelocity() const
{
	const physx::PxRigidDynamic* Dynamic = GetRigidDynamic();
	if (!Dynamic) return FVector::ZeroVector;

	return PhysXConvert::ToFVector(Dynamic->getLinearVelocity());
}

void FBodyInstance::AddForceAtLocation(const FVector& Force, const FVector& WorldLocation)
{
	if (physx::PxRigidDynamic* Dynamic = GetRigidDynamic())
	{
		physx::PxRigidBodyExt::addForceAtPos(
			*Dynamic,
			PhysXConvert::ToPxVec3(Force),
			PhysXConvert::ToPxVec3(WorldLocation),
			physx::PxForceMode::eFORCE,
			true
		);
	}
}

void FBodyInstance::AddTorque(const FVector& Torque)
{
	if (physx::PxRigidDynamic* Dynamic = GetRigidDynamic())
	{
		Dynamic->addTorque(
			PhysXConvert::ToPxVec3(Torque),
			physx::PxForceMode::eFORCE,
			true
		);
	}
}

void FBodyInstance::SetAngularVelocity(const FVector& Velocity)
{
	if (physx::PxRigidDynamic* Dynamic = GetRigidDynamic())
	{
		Dynamic->setAngularVelocity(PhysXConvert::ToPxVec3(Velocity));
		WakeUp();
	}
}

FVector FBodyInstance::GetAngularVelocity() const
{
	if (physx::PxRigidDynamic* Dynamic = GetRigidDynamic())
	{
		return PhysXConvert::ToFVector(Dynamic->getAngularVelocity());
	}

	return FVector::ZeroVector;
}

void FBodyInstance::SetMass(float NewMass)
{
	Mass = NewMass > 0.001f ? NewMass : 0.001f;

	if (physx::PxRigidDynamic* Dynamic = GetRigidDynamic())
	{
		physx::PxVec3 LocalCOM = PhysXConvert::ToPxVec3(CenterOfMassOffset);
		physx::PxRigidBodyExt::setMassAndUpdateInertia(*Dynamic, Mass, &LocalCOM);
		Dynamic->setCMassLocalPose(physx::PxTransform(LocalCOM));
		WakeUp();
	}
}

float FBodyInstance::GetMass() const
{
	if (physx::PxRigidDynamic* Dynamic = GetRigidDynamic())
	{
		return Dynamic->getMass();
	}

	return Mass;
}

void FBodyInstance::SetCenterOfMass(const FVector& LocalOffset)
{
	CenterOfMassOffset = LocalOffset;

	if (physx::PxRigidDynamic* Dynamic = GetRigidDynamic())
	{
		physx::PxTransform MassPose = Dynamic->getCMassLocalPose();
		MassPose.p = PhysXConvert::ToPxVec3(LocalOffset);
		Dynamic->setCMassLocalPose(MassPose);
		WakeUp();
	}
}

FVector FBodyInstance::GetCenterOfMass() const
{
	if (physx::PxRigidDynamic* Dynamic = GetRigidDynamic())
	{
		const physx::PxTransform MassPose = Dynamic->getCMassLocalPose();
		return PhysXConvert::ToFVector(MassPose.p);
	}

	return CenterOfMassOffset;
}

void FBodyInstance::ClearPhysicsPointers()
{
	RigidActor = nullptr;
	Shapes.clear();
}

bool BuildBodyInstanceInitDescFromPrimitive(UPrimitiveComponent* Comp, FBodyInstanceInitDesc& OutDesc)
{
    if (!IsValid(Comp)) return false;

    FBodyInstance& Body = Comp->GetBodyInstance();

    Body.OwnerComponent = Comp;
    Body.OwnerSkeletalComponent = nullptr;

    OutDesc = FBodyInstanceInitDesc();

    FTransform WorldTransform = FTransform::FromMatrixWithScale(Comp->GetWorldMatrix());
    WorldTransform.Scale = FVector::OneVector;

    OutDesc.WorldTransform = WorldTransform;

    OutDesc.bSimulatePhysics = Body.bSimulatePhysics;
    OutDesc.bKinematic = Body.bKinematic;

    OutDesc.CollisionEnabled = Body.CollisionEnabled;
    OutDesc.ObjectType = Body.ObjectType;
    OutDesc.ResponseContainer = Body.ResponseContainer;

    OutDesc.Mass = Body.Mass > 0.001f ? Body.Mass : 0.001f;
    OutDesc.CenterOfMassOffset = Body.CenterOfMassOffset;

    if (OutDesc.CollisionEnabled == ECollisionEnabled::NoCollision)
    {
        return false;
    }

    FBodyShapeDesc Shape;
    Shape.LocalTransform = FTransform();

    if (UBoxComponent* Box = Cast<UBoxComponent>(Comp))
    {
        Shape.ShapeType = EBodyInstanceShapeType::Box;
        Shape.BoxHalfExtent = Box->GetScaledBoxExtent();

        Shape.BoxHalfExtent.X = std::max(Shape.BoxHalfExtent.X, 0.001f);
        Shape.BoxHalfExtent.Y = std::max(Shape.BoxHalfExtent.Y, 0.001f);
        Shape.BoxHalfExtent.Z = std::max(Shape.BoxHalfExtent.Z, 0.001f);

        OutDesc.Shapes.push_back(Shape);
    }
    else if (USphereComponent* Sphere = Cast<USphereComponent>(Comp))
    {
        Shape.ShapeType = EBodyInstanceShapeType::Sphere;
        Shape.SphereRadius = std::max(Sphere->GetScaledSphereRadius(), 0.001f);

        OutDesc.Shapes.push_back(Shape);
    }
    else if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Comp))
    {
        const float Radius = std::max(Capsule->GetScaledCapsuleRadius(), 0.001f);
        const float HalfHeight = std::max(Capsule->GetScaledCapsuleHalfHeight(), Radius + 0.001f);

        Shape.ShapeType = EBodyInstanceShapeType::Capsule;
        Shape.CapsuleRadius = Radius;
        Shape.CapsuleHalfHeight = HalfHeight;

        OutDesc.Shapes.push_back(Shape);
    }

    return !OutDesc.Shapes.empty();
}
