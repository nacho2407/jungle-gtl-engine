#include "SceneComponent.h"


DEFINE_CLASS(USceneComponent, UActorComponent)

void USceneComponent::AttachToComponent(USceneComponent* InParent)
{
	if (!InParent || InParent == this) return;
	SetParent(InParent);
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
	RelativeLocation = NewLocation;
	MarkTransformDirty();
}

void USceneComponent::SetRelativeRotation(const FVector& NewRotation)
{
	RelativeRotation = NewRotation;
	MarkTransformDirty();
}


void USceneComponent::SetRelativeScale(const FVector& NewScale)
{
	RelativeScale3D = NewScale;
	MarkTransformDirty();
}


void USceneComponent::MarkTransformDirty()
{
	bTransformDirty = true;
	for (auto* Child : ChildComponents)
	{
		Child->MarkTransformDirty();
	}
}

const FMatrix& USceneComponent::GetWorldMatrix() const
{
	if (bTransformDirty == true)
	{
		UpdateWorldMatrix();
	}

	return CachedWorldMatrix;
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
	SetRelativeLocation(RelativeLocation + Delta);
}

void USceneComponent::MoveLocal(const FVector& Delta)
{
	FVector Forward = GetForwardVector();
	FVector Right = GetRightVector();
	FVector Up = GetUpVector();

	SetRelativeLocation(RelativeLocation
		+ Forward * Delta.X
		+ Right * Delta.Y
		+ Up * Delta.Z);
}

void USceneComponent::Rotate(float DeltaYaw, float DeltaPitch)
{
	RelativeRotation.Z += DeltaYaw;
	RelativeRotation.Y += DeltaPitch;
	RelativeRotation.Y = Clamp(RelativeRotation.Y, -89.9f, 89.9f);

	RelativeRotation.X = 0.0f;

	SetRelativeRotation(RelativeRotation);
}

FMatrix USceneComponent::GetRelativeMatrix() const
{
	return FTransform(RelativeLocation, RelativeRotation, RelativeScale3D).ToMatrix();
}