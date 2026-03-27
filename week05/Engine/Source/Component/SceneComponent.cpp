#include "SceneComponent.h"
#include "Object/Class.h"

IMPLEMENT_RTTI(USceneComponent, UActorComponent)

void USceneComponent::SetRelativeTransform(const FTransform& InTransform)
{
	RelativeTransform = InTransform;
	MarkTransformDirty();
}

void USceneComponent::SetRelativeLocation(const FVector& InLocation)
{
	RelativeTransform.SetTranslation(InLocation);
	MarkTransformDirty();
}

void USceneComponent::AttachTo(USceneComponent* InParent)
{
	if (AttachParent == InParent)
	{
		return;
	}

	DetachFromParent();

	AttachParent = InParent;
	if (AttachParent)
	{
		AttachParent->AttachChildren.push_back(this);
	}

	MarkTransformDirty();
}

void USceneComponent::DetachFromParent()
{
	if (AttachParent == nullptr)
	{
		return;
	}

	auto& Siblings = AttachParent->AttachChildren;
	std::erase(Siblings, this);
	AttachParent = nullptr;

	MarkTransformDirty();
}

const FMatrix& USceneComponent::GetWorldTransform() const
{
	if (bWorldTransformDirty)
	{
		UpdateWorldTransform();
	}
	return CachedWorldTransform;
}

FVector USceneComponent::GetWorldLocation() const
{
	return GetWorldTransform().GetTranslation();
}

void USceneComponent::MarkTransformDirty()
{
	bWorldTransformDirty = true;

	for (USceneComponent* Child : AttachChildren)
	{
		if (Child)
		{
			Child->MarkTransformDirty();
		}
	}
}

void USceneComponent::UpdateWorldTransform() const
{
	CachedWorldTransform = RelativeTransform.ToMatrixWithScale();
	if (AttachParent)
	{
		CachedWorldTransform = CachedWorldTransform * AttachParent->GetWorldTransform();
	}
	bWorldTransformDirty = false;
}
