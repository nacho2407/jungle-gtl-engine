#include "SceneComponent.h"
#include "Object/ObjectFactory.h"
#include <GameFramework/World.h>

IMPLEMENT_CLASS(USceneComponent, UActorComponent)

static void NotifyOctreeTransformChanged(USceneComponent* Comp)
{
    if (!Comp) return;

    AActor* OwnerActor = Comp->GetOwner();
    if (!OwnerActor) return;

    UWorld* World = OwnerActor->GetWorld();
    if (!World) return;
	
    World->UpdateActorInOctree(OwnerActor);
}

void USceneComponent::AttachToComponent(USceneComponent* InParent)
{
	if (!InParent || InParent == this) return;
	SetParent(InParent);
}

void USceneComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	if (bCachedEulerDirty)
	{
		CachedEditRotator = RelativeTransform.GetRotator();
		bCachedEulerDirty = false;
	}
	OutProps.push_back({ "Location", EPropertyType::Vec3, &RelativeTransform.Location, 0.0f, 0.0f, 0.1f });
	OutProps.push_back({ "Rotation", EPropertyType::Rotator, &CachedEditRotator, 0.0f, 0.0f, 0.1f });
	OutProps.push_back({ "Scale", EPropertyType::Vec3, &RelativeTransform.Scale, 0.0f, 0.0f, 0.1f });
}

void USceneComponent::PostEditProperty(const char* PropertyName)
{
	bool bApplyChangeToPartition = (strcmp(PropertyName, "Location") == 0
								|| strcmp(PropertyName, "Rotation") == 0
								|| strcmp(PropertyName, "Scale") == 0);

	if (strcmp(PropertyName, "Rotation") == 0)
	{
		ApplyCachedEditRotator();
	}
	else
	{
		MarkTransformDirty();
	}

	if (bApplyChangeToPartition)
	{
		NotifyOctreeTransformChanged(this);
	}
}

USceneComponent::USceneComponent()
{
	CachedWorldMatrix = FMatrix::Identity;

	bTransformDirty = true;
	UpdateWorldMatrix();
}

USceneComponent::~USceneComponent()
{
	if (ParentComponent != nullptr)
	{
		ParentComponent->RemoveChild(this);
		ParentComponent = nullptr;
	}

	for (auto* Child : ChildComponents)
	{
		if (Child)
		{
			Child->ParentComponent = nullptr;
			Child->MarkTransformDirty();
		}
	}
	ChildComponents.clear();
}

void USceneComponent::SetParent(USceneComponent* NewParent)
{
	if (NewParent == ParentComponent || NewParent == this)
	{
		return;
	}

	if (ParentComponent)
	{
		ParentComponent->RemoveChild(this);
	}

	ParentComponent = NewParent;
	if (ParentComponent)
	{
		if (ParentComponent->ContainsChild(this) == false)
		{
			ParentComponent->ChildComponents.push_back(this);
		}
	}

	// 부모 변경 시 자신 및 하위 자식의 월드 행렬을 갱신하도록 dirty 마킹
	MarkTransformDirty();
}

void USceneComponent::AddChild(USceneComponent* NewChild)
{
	if (NewChild == nullptr)
	{
		return;
	}

	NewChild->SetParent(this);
}

void USceneComponent::RemoveChild(USceneComponent* Child)
{
	if (Child == nullptr)
	{
		return;
	}

	auto iter = std::find(ChildComponents.begin(), ChildComponents.end(), Child);

	if (iter != ChildComponents.end())
	{
		if ((*iter)->ParentComponent == this)
		{
			(*iter)->ParentComponent = nullptr;
		}

		ChildComponents.erase(iter);
	}
}

bool USceneComponent::ContainsChild(const USceneComponent* Child) const
{
	if (Child == nullptr)
	{
		return false;
	}

	return std::find(ChildComponents.begin(),
		ChildComponents.end(), Child) != ChildComponents.end();
}

void USceneComponent::UpdateWorldMatrix() const
{
	if (bTransformDirty == false)
	{
		return;
	}

	FMatrix RelativeMatrix = GetRelativeMatrix();

	if (ParentComponent != nullptr)
	{
		CachedWorldMatrix = RelativeMatrix * ParentComponent->GetWorldMatrix();
	}
	else
	{
		CachedWorldMatrix = RelativeMatrix;
	}

	bTransformDirty = false;
}

void USceneComponent::AddWorldOffset(const FVector& WorldDelta)
{
	if (ParentComponent == nullptr)
	{
		Move(WorldDelta);
	}
	else
	{
		const FMatrix& parentWorldMatrix = ParentComponent->GetWorldMatrix();

		FMatrix parentWorldInverseMatrix = parentWorldMatrix.GetInverse();

		FVector localDelta = parentWorldInverseMatrix.TransformVector(WorldDelta);

		Move(localDelta);
	}
}

void USceneComponent::SetRelativeLocation(const FVector& NewLocation)
{
	RelativeTransform.Location = NewLocation;
	MarkTransformDirty();
	NotifyOctreeTransformChanged(this);
}

void USceneComponent::SetRelativeRotation(const FRotator& NewRotation)
{
	CachedEditRotator = NewRotation.GetClamped();
	bCachedEulerDirty = false;
	RelativeTransform.SetRotation(NewRotation);
	MarkTransformDirty();
	NotifyOctreeTransformChanged(this);
}

void USceneComponent::SetRelativeRotation(const FQuat& NewRotation)
{
	bCachedEulerDirty = true;
	RelativeTransform.SetRotation(NewRotation);
	MarkTransformDirty();
	NotifyOctreeTransformChanged(this);
}

void USceneComponent::SetRelativeRotation(const FVector& EulerRotation)
{
	FRotator Rot(EulerRotation);
	CachedEditRotator = Rot;
	bCachedEulerDirty = false;
	RelativeTransform.SetRotation(Rot);
	MarkTransformDirty();
	NotifyOctreeTransformChanged(this);
}


void USceneComponent::SetRelativeScale(const FVector& NewScale)
{
	RelativeTransform.Scale = NewScale;
	MarkTransformDirty();
	NotifyOctreeTransformChanged(this);
}


void USceneComponent::MarkTransformDirty()
{
	bTransformDirty = true;
	bInverseWorldDirty = true;
	OnTransformDirty();
	for (auto* Child : ChildComponents)
	{
		Child->MarkTransformDirty();
	}
}

void USceneComponent::OnTransformDirty()
{
}

FRotator& USceneComponent::GetCachedEditRotator()
{
	if (bCachedEulerDirty)
	{
		CachedEditRotator = RelativeTransform.GetRotator();
		bCachedEulerDirty = false;
	}
	return CachedEditRotator;
}

void USceneComponent::SetRelativeRotationWithEulerHint(const FQuat& NewQuat, const FRotator& EulerHint)
{
	CachedEditRotator = EulerHint.GetClamped();
	bCachedEulerDirty = false;
	RelativeTransform.SetRotation(NewQuat);
	MarkTransformDirty();
    NotifyOctreeTransformChanged(this);
}

void USceneComponent::ApplyCachedEditRotator()
{
	CachedEditRotator = CachedEditRotator.GetClamped();
	bCachedEulerDirty = false;
	RelativeTransform.SetRotation(CachedEditRotator);
	MarkTransformDirty();
    NotifyOctreeTransformChanged(this);
}

const FMatrix& USceneComponent::GetWorldMatrix() const
{
	if (bTransformDirty == true)
	{
		UpdateWorldMatrix();
	}

	return CachedWorldMatrix;
}

const FMatrix& USceneComponent::GetWorldInverseMatrix() const
{
	GetWorldMatrix();

	if (bInverseWorldDirty == true)
	{
		CachedInverseWorldMatrix = CachedWorldMatrix.GetInverse();
		bInverseWorldDirty = false;
	}
	return CachedInverseWorldMatrix;
}

void USceneComponent::SetWorldLocation(FVector NewWorldLocation)
{
	if (ParentComponent != nullptr)
	{
		const FMatrix& parentWorldInverseMatrix = ParentComponent->GetWorldMatrix().GetInverse();

		FVector newRelativeLocation = NewWorldLocation * parentWorldInverseMatrix;

		SetRelativeLocation(newRelativeLocation);
	}

	else
	{
		SetRelativeLocation(NewWorldLocation);
	}
}

FVector USceneComponent::GetWorldLocation() const
{
	const FMatrix& WorldMatrix = GetWorldMatrix();
	return FVector(WorldMatrix.M[3][0], WorldMatrix.M[3][1], WorldMatrix.M[3][2]);
}

FVector USceneComponent::GetWorldScale() const
{
	const FMatrix& WorldMatrix = GetWorldMatrix();

	float ScaleX = FVector(WorldMatrix.M[0][0], WorldMatrix.M[0][1], WorldMatrix.M[0][2]).Length();
	float ScaleY = FVector(WorldMatrix.M[1][0], WorldMatrix.M[1][1], WorldMatrix.M[1][2]).Length();
	float ScaleZ = FVector(WorldMatrix.M[2][0], WorldMatrix.M[2][1], WorldMatrix.M[2][2]).Length();

	return FVector(ScaleX, ScaleY, ScaleZ);
}

FVector USceneComponent::GetForwardVector() const
{
	const FMatrix& Matrix = GetWorldMatrix();
	FVector Forward(Matrix.M[0][0], Matrix.M[0][1], Matrix.M[0][2]);
	Forward.Normalize();
	return Forward;
}

FVector USceneComponent::GetRightVector() const
{
	const FMatrix& Matrix = GetWorldMatrix();
	FVector Right(Matrix.M[1][0], Matrix.M[1][1], Matrix.M[1][2]);
	Right.Normalize();
	return Right;
}

FVector USceneComponent::GetUpVector() const
{
	const FMatrix& Matrix = GetWorldMatrix();
	FVector Up(Matrix.M[2][0], Matrix.M[2][1], Matrix.M[2][2]);
	Up.Normalize();
	return Up;
}

void USceneComponent::Move(const FVector& Delta)
{
	SetRelativeLocation(RelativeTransform.Location + Delta);
}

void USceneComponent::MoveLocal(const FVector& Delta)
{
	FVector Forward = GetForwardVector();
	FVector Right = GetRightVector();
	FVector Up = GetUpVector();

	SetRelativeLocation(RelativeTransform.Location
		+ Forward * Delta.X
		+ Right * Delta.Y
		+ Up * Delta.Z);
}

void USceneComponent::Rotate(float DeltaYaw, float DeltaPitch)
{
	FRotator Rot = GetRelativeRotation();
	Rot.Yaw += DeltaYaw;
	Rot.Pitch += DeltaPitch;
	Rot.Pitch = Clamp(Rot.Pitch, -89.9f, 89.9f);
	Rot.Roll = 0.0f;

	SetRelativeRotation(Rot);
}

FMatrix USceneComponent::GetRelativeMatrix() const
{
	return RelativeTransform.ToMatrix();
}