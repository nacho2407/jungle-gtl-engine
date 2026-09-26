#include "SceneComponent.h"
#include "Object/Class.h"
#include "Serializer/Archive.h"

IMPLEMENT_RTTI(USceneComponent, UActorComponent)

void USceneComponent::DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const
{
	UActorComponent::DuplicateShallow(DuplicatedObject, Context);

	USceneComponent* DuplicatedSceneComponent = static_cast<USceneComponent*>(DuplicatedObject);
	DuplicatedSceneComponent->RelativeTransform = RelativeTransform;
	DuplicatedSceneComponent->AttachParent = nullptr;
	DuplicatedSceneComponent->AttachChildren.clear();
	DuplicatedSceneComponent->bWorldTransformDirty = true;
}

void USceneComponent::FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
	UActorComponent::FixupDuplicatedReferences(DuplicatedObject, Context);

	USceneComponent* DuplicatedSceneComponent = static_cast<USceneComponent*>(DuplicatedObject);
	if (USceneComponent* DuplicatedParent = Context.FindDuplicate(AttachParent.Get()))
	{
		DuplicatedSceneComponent->AttachTo(DuplicatedParent);
	}
	else
	{
		DuplicatedSceneComponent->DetachFromParent();
	}
}

void USceneComponent::PostDuplicate(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
	USceneComponent* DuplicatedSceneComponent = static_cast<USceneComponent*>(DuplicatedObject);
	DuplicatedSceneComponent->MarkTransformDirty();
}

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

void USceneComponent::Serialize(FArchive& Ar)
{
	UActorComponent::Serialize(Ar);

	if (Ar.IsSaving())
	{
		FVector Location = RelativeTransform.GetTranslation();
		FVector Rotation = RelativeTransform.Rotator().Euler();
		FVector Scale = RelativeTransform.GetScale3D();

		Ar.Serialize("Location", Location);
		Ar.Serialize("Rotation", Rotation);
		Ar.Serialize("Scale", Scale);
	}
	else
	{
		FTransform Transform = RelativeTransform;

		if (Ar.Contains("Location"))
		{
			FVector Location;
			Ar.Serialize("Location", Location);
			Transform.SetTranslation(Location);
		}
		if (Ar.Contains("Rotation"))
		{
			FVector Rotation;
			Ar.Serialize("Rotation", Rotation);
			Transform.SetRotation(FRotator::MakeFromEuler(Rotation));
		}
		if (Ar.Contains("Scale"))
		{
			FVector Scale;
			Ar.Serialize("Scale", Scale);
			Transform.SetScale3D(Scale);
		}

		SetRelativeTransform(Transform);
	}
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
