#include "BodyInstance.h"

#include "PhysXTypeConversions.h"
#include "GameFramework/AActor.h"
#include "Core/Logging/Log.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Component/Shape/SphereComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Mesh/Static/StaticMesh.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/ShapeElem.h"
#include "PhysicsEngine/BodySetup.h"
#include <algorithm>

namespace
{
	constexpr float GClothBodyCollisionMinExtent = 0.001f;

	bool ShouldCreateBodyShape(const FKShapeElem& ShapeElem)
	{
		return ShapeElem.GetCollisionEnabled() != ECollisionEnabled::NoCollision;
	}

	/**
	 * @brief PhysX geometry type warning을 제한적으로 기록합니다
	 *
	 * @param GeometryType 지원하지 않는 PhysX geometry type
	 */
	void LogUnsupportedClothBodyGeometryOnce(physx::PxGeometryType::Enum GeometryType)
	{
		static bool bLoggedUnsupportedGeometry = false;
		if (bLoggedUnsupportedGeometry)
		{
			return;
		}

		bLoggedUnsupportedGeometry = true;
		UE_LOG("[BodyInstance] Unsupported cloth body collision geometry skipped: type=%d", static_cast<int32>(GeometryType));
	}

	/**
	 * @brief PhysX transform의 지정 축을 engine vector로 변환합니다
	 *
	 * @param Pose 축 방향을 가진 PhysX transform
	 *
	 * @param Axis 기준 축 vector
	 *
	 * @return world 기준 단위 축 방향
	 */
	FVector GetShapeWorldAxis(const physx::PxTransform& Pose, const physx::PxVec3& Axis)
	{
		return PhysXConvert::ToFVector(Pose.q.rotate(Axis)).GetSafeNormal(GClothBodyCollisionMinExtent, FVector::UpVector);
	}
}

physx::PxRigidDynamic* FBodyInstance::GetRigidDynamic() const
{
	return RigidActor ? RigidActor->is<physx::PxRigidDynamic>() : nullptr;
}

physx::PxRigidStatic* FBodyInstance::GetRigidStatic() const
{
	return RigidActor ? RigidActor->is<physx::PxRigidStatic>() : nullptr;
}

AActor* FBodyInstance::GetOwnerActor() const
{
	if (OwnerComponent)
	{
		return OwnerComponent->GetOwner();
	}

	if (OwnerSkeletalComponent)
	{
		return OwnerSkeletalComponent->GetOwner();
	}

	return nullptr;
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

void FBodyInstance::SetKinematic(bool bInKinematic)
{
	bKinematic = bInKinematic;

	physx::PxRigidDynamic* Dynamic = GetRigidDynamic();
	if (!Dynamic) return;

	Dynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, bInKinematic);

	if (bInKinematic)
	{
		Dynamic->setKinematicTarget(PhysXConvert::ToPxTransform(GetBodyTransform()));
	}
	else
	{
		Dynamic->wakeUp();
	}
}

void FBodyInstance::SetGravityEnabled(bool bInEnableGravity)
{
	bEnableGravity = bInEnableGravity;

	physx::PxRigidDynamic* Dynamic = GetRigidDynamic();
	if (!Dynamic) return;

	Dynamic->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !bInEnableGravity);

	if (bInEnableGravity)
	{
		Dynamic->wakeUp();
	}
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

		physx::PxVec3 Inertia = Dynamic->getMassSpaceInertiaTensor();
		Inertia.x *= std::max(InertiaTensorScale.X, 0.001f);
		Inertia.y *= std::max(InertiaTensorScale.Y, 0.001f);
		Inertia.z *= std::max(InertiaTensorScale.Z, 0.001f);
		Dynamic->setMassSpaceInertiaTensor(Inertia);

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

void FBodyInstance::AppendClothCollisionPrimitives(TArray<FClothCollisionPrimitive>& OutPrimitives) const
{
	if (!IsValidBodyInstance() || !RigidActor || Shapes.empty())
	{
		return;
	}

	const physx::PxTransform ActorPose = RigidActor->getGlobalPose();
	for (physx::PxShape* Shape : Shapes)
	{
		if (!Shape)
		{
			continue;
		}

		const physx::PxGeometryHolder Geometry = Shape->getGeometry();
		const physx::PxTransform ShapePose = ActorPose * Shape->getLocalPose();
		switch (Geometry.getType())
		{
		case physx::PxGeometryType::eSPHERE:
		{
			const physx::PxSphereGeometry& Sphere = Geometry.sphere();
			if (Sphere.radius <= GClothBodyCollisionMinExtent)
			{
				continue;
			}

			// PhysX sphere 중심은 shape pose 위치와 같음
			FClothCollisionPrimitive Primitive;
			Primitive.Type = EClothCollisionPrimitiveType::Sphere;
			Primitive.Source = EClothCollisionPrimitiveSource::Body;
			Primitive.Center = PhysXConvert::ToFVector(ShapePose.p);
			Primitive.Radius = Sphere.radius;
			OutPrimitives.push_back(Primitive);
			break;
		}

		case physx::PxGeometryType::eBOX:
		{
			const physx::PxBoxGeometry& Box = Geometry.box();
			if (Box.halfExtents.x <= GClothBodyCollisionMinExtent
				|| Box.halfExtents.y <= GClothBodyCollisionMinExtent
				|| Box.halfExtents.z <= GClothBodyCollisionMinExtent)
			{
				continue;
			}

			// PhysX box half extent와 shape pose 축을 world 기준으로 snapshot화
			FClothCollisionPrimitive Primitive;
			Primitive.Type = EClothCollisionPrimitiveType::Box;
			Primitive.Source = EClothCollisionPrimitiveSource::Body;
			Primitive.Center = PhysXConvert::ToFVector(ShapePose.p);
			Primitive.BoxExtent = FVector(Box.halfExtents.x, Box.halfExtents.y, Box.halfExtents.z);
			Primitive.BoxAxisX = GetShapeWorldAxis(ShapePose, physx::PxVec3(1.0f, 0.0f, 0.0f));
			Primitive.BoxAxisY = GetShapeWorldAxis(ShapePose, physx::PxVec3(0.0f, 1.0f, 0.0f));
			Primitive.BoxAxisZ = GetShapeWorldAxis(ShapePose, physx::PxVec3(0.0f, 0.0f, 1.0f));
			OutPrimitives.push_back(Primitive);
			break;
		}

		case physx::PxGeometryType::eCAPSULE:
		{
			const physx::PxCapsuleGeometry& Capsule = Geometry.capsule();
			if (Capsule.radius <= GClothBodyCollisionMinExtent)
			{
				continue;
			}

			// PhysX capsule은 x축 기준이며 halfHeight는 sphere center 간 절반 거리
			const FVector Center = PhysXConvert::ToFVector(ShapePose.p);
			const FVector Axis = GetShapeWorldAxis(ShapePose, physx::PxVec3(1.0f, 0.0f, 0.0f));
			const float SegmentHalfHeight = (std::max)(0.0f, Capsule.halfHeight);

			FClothCollisionPrimitive Primitive;
			Primitive.Type = EClothCollisionPrimitiveType::Capsule;
			Primitive.Source = EClothCollisionPrimitiveSource::Body;
			Primitive.Center = Center;
			Primitive.Axis = Axis;
			Primitive.CapsuleStart = Center - Axis * SegmentHalfHeight;
			Primitive.CapsuleEnd = Center + Axis * SegmentHalfHeight;
			Primitive.Radius = Capsule.radius;
			Primitive.HalfHeight = SegmentHalfHeight;
			OutPrimitives.push_back(Primitive);
			break;
		}

		case physx::PxGeometryType::eCONVEXMESH:
			LogUnsupportedClothBodyGeometryOnce(Geometry.getType());
			break;

		default:
			LogUnsupportedClothBodyGeometryOnce(Geometry.getType());
			break;
		}
	}
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
    const FVector WorldScale = WorldTransform.Scale;
    WorldTransform.Scale = FVector::OneVector;

    OutDesc.WorldTransform = WorldTransform;

    OutDesc.bKinematic = Body.bKinematic;
    OutDesc.bSimulatePhysics = Body.bSimulatePhysics || Body.bKinematic;

    OutDesc.CollisionEnabled = Body.CollisionEnabled;
    OutDesc.ObjectType = Body.ObjectType;
    OutDesc.ResponseContainer = Body.ResponseContainer;
	OutDesc.bIgnoreSameOwner = Body.bIgnoreSameOwner;

    Body.Mass = Comp->GetMass();
    Body.CenterOfMassOffset = Comp->GetCenterOfMass();

    OutDesc.Mass = Body.Mass > 0.001f ? Body.Mass : 0.001f;
    OutDesc.CenterOfMassOffset = Body.CenterOfMassOffset;
    OutDesc.LinearDamping = Body.LinearDamping;
    OutDesc.AngularDamping = Body.AngularDamping;
    OutDesc.bEnableGravity = Body.bEnableGravity;
    OutDesc.InertiaTensorScale = Body.InertiaTensorScale;

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
    else if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Comp))
    {
        UStaticMesh* Mesh = StaticMeshComp->GetStaticMesh();
        if (Mesh)
        {
            Mesh->EnsureDefaultBodySetup();
        }

        const UBodySetup* BodySetup = Mesh ? Mesh->GetBodySetup() : nullptr;
        if (!BodySetup || BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Disabled)
        {
            return false;
        }

        const FBodySetupPhysicsInfo& PhysicsInfo = BodySetup->GetPhysicsInfo();
        OutDesc.Mass = BodySetup->CalculateMass(WorldScale);
        OutDesc.CenterOfMassOffset = PhysicsInfo.CenterOfMassOffset;
        OutDesc.LinearDamping = PhysicsInfo.LinearDamping;
        OutDesc.AngularDamping = PhysicsInfo.AngularDamping;
        OutDesc.InertiaTensorScale = PhysicsInfo.InertiaTensorScale;

        const FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();

        for (const FKSphereElem& Sphere : AggGeom.SphereElems)
        {
            if (!ShouldCreateBodyShape(Sphere))
            {
                continue;
            }

            const FKSphereElem ScaledSphere = Sphere.GetFinalScaled(WorldScale, FTransform());

            FBodyShapeDesc MeshShape;
            MeshShape.ShapeType = EBodyInstanceShapeType::Sphere;
            MeshShape.LocalTransform = ScaledSphere.GetTransform();
            MeshShape.SphereRadius = std::max(ScaledSphere.Radius, 0.001f);

            OutDesc.Shapes.push_back(MeshShape);
        }

        for (const FKBoxElem& Box : AggGeom.BoxElems)
        {
            if (!ShouldCreateBodyShape(Box))
            {
                continue;
            }

            const FKBoxElem ScaledBox = Box.GetFinalScaled(WorldScale, FTransform());

            FBodyShapeDesc MeshShape;
            MeshShape.ShapeType = EBodyInstanceShapeType::Box;
            MeshShape.LocalTransform = ScaledBox.GetTransform();
            MeshShape.BoxHalfExtent = FVector(
                std::max(ScaledBox.X * 0.5f, 0.001f),
                std::max(ScaledBox.Y * 0.5f, 0.001f),
                std::max(ScaledBox.Z * 0.5f, 0.001f)
            );

            OutDesc.Shapes.push_back(MeshShape);
        }

        for (const FKSphylElem& Sphyl : AggGeom.SphylElems)
        {
            if (!ShouldCreateBodyShape(Sphyl))
            {
                continue;
            }

            const FKSphylElem ScaledSphyl = Sphyl.GetFinalScaled(WorldScale, FTransform());

            FBodyShapeDesc MeshShape;
            MeshShape.ShapeType = EBodyInstanceShapeType::Capsule;
            MeshShape.LocalTransform = ScaledSphyl.GetTransform();
            MeshShape.CapsuleRadius = std::max(ScaledSphyl.Radius, 0.001f);
            MeshShape.CapsuleHalfHeight = std::max(
                ScaledSphyl.Length * 0.5f + ScaledSphyl.Radius,
                MeshShape.CapsuleRadius + 0.001f
            );

            OutDesc.Shapes.push_back(MeshShape);
        }

		for (const FKConvexElem& Convex : AggGeom.ConvexElems)
		{
			if (Convex.GetCollisionEnabled() == ECollisionEnabled::NoCollision)
			{
				continue;
			}

			if (Convex.VertexData.size() < 4)
			{
				continue;
			}

			FBodyShapeDesc MeshShape;
			MeshShape.ShapeType = EBodyInstanceShapeType::Convex;

			const FTransform ConvexLocalTM = Convex.GetTransform();

			// Actor의 WorldTransform에서는 Scale을 제거하고 있으므로,
			// shape local 위치에는 component scale을 반영해야 함.
			MeshShape.LocalTransform = ConvexLocalTM;
			MeshShape.LocalTransform.Location = ConvexLocalTM.Location * WorldScale;

			// PxTransform에는 Scale이 없으므로 local transform scale은 제거.
			// 실제 convex 크기는 PxConvexMeshGeometry의 PxMeshScale로 넘긴다.
			MeshShape.LocalTransform.Scale = FVector::OneVector;

			const FVector RawScale = ConvexLocalTM.Scale * WorldScale;
			const FVector AbsScale = RawScale.GetAbs();

			MeshShape.ConvexScale = FVector(
				std::max(AbsScale.X, 0.001f),
				std::max(AbsScale.Y, 0.001f),
				std::max(AbsScale.Z, 0.001f)
			);

			MeshShape.ConvexVertices = Convex.VertexData;
			MeshShape.ConvexIndices = Convex.IndexData;

			OutDesc.Shapes.push_back(MeshShape);
		}
    }

    OutDesc.bEnableGravity = Body.bEnableGravity;

    return !OutDesc.Shapes.empty();
}
