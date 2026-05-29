#include "Physics/BodyInstance.h"
#include "Physics/PhysXConversions.h"

#include "Component/PrimitiveComponent.h"

AActor* FBodyInstance::GetOwnerActor() const
{
	return OwnerComponent ? OwnerComponent->GetOwner() : nullptr;
}

void FBodyInstance::SyncFromPhysics()
{
	if (!OwnerComponent || !Body) return;

	physx::PxRigidDynamic* DynamicBody = Body->is<physx::PxRigidDynamic>();
	if (!DynamicBody) return;

	const physx::PxTransform Pose = DynamicBody->getGlobalPose();
	OwnerComponent->SetWorldLocation(FromPxVec3(Pose.p));
	OwnerComponent->SetRelativeRotation(FromPxQuat(Pose.q));
}

void FBodyInstance::SyncToPhysics()
{
	if (!OwnerComponent || !Body) return;

	Body->setGlobalPose(ToPxTransform(
		OwnerComponent->GetWorldLocation(),
		OwnerComponent->GetWorldRotation().ToQuaternion()));
}
