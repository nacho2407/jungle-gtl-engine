#include "SceneComponent.h"


DEFINE_CLASS(USceneComponent, UActorComponent)

USceneComponent::USceneComponent()
{
	CachedWorldMatrix = FMatrix::Identity;

	bUpdateFlag = true;
	UpdateWorldMatrix();

}

USceneComponent::~USceneComponent()
{
	if (ParentComponent != nullptr)
	{
		ParentComponent->RemoveChild(this);
		ParentComponent = nullptr;
	}

	for (auto* child : ChildComponents)
	{
		if (child)
		{
			child->ParentComponent = nullptr;
			child->SetUpdateFlag();
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

	bool result = std::find(ChildComponents.begin(),
		ChildComponents.end(), Child) != ChildComponents.end();

	return result;
}

void USceneComponent::UpdateWorldMatrix()
{
	if (bUpdateFlag == false)
	{
		return;
	}

	FMatrix relativeMatrix = GetRelativeMatrixTemp();

	if (ParentComponent != nullptr)
	{
		CachedWorldMatrix = relativeMatrix * ParentComponent->GetWorldMatrix();
	}
	else
	{
		CachedWorldMatrix = relativeMatrix;
	}

	bUpdateFlag = false;
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


void USceneComponent::SetRelativeLocation(const FVector NewLocation)
{
	RelativeLocation = NewLocation;
	SetUpdateFlag();
}

void USceneComponent::SetRelativeRotation(const FVector NewRotation)
{
	RelativeRotation = NewRotation;
	SetUpdateFlag();
}


void USceneComponent::SetRelativeScale(const FVector NewScale)
{
	RelativeScale3D = NewScale;
	SetUpdateFlag();
}


void USceneComponent::SetUpdateFlag()
{
	bUpdateFlag = true;
	for (auto* child : ChildComponents)
	{
		child->SetUpdateFlag();
	}
}

const FMatrix& USceneComponent::GetWorldMatrix()
{
	if (bUpdateFlag == true)
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

FVector USceneComponent::GetWorldLocation()
{
	const FMatrix& worldMatrix = GetWorldMatrix();
	return FVector(worldMatrix.M[3][0], worldMatrix.M[3][1], worldMatrix.M[3][2]);
}

FVector USceneComponent::GetForwardVector()
{
	const FMatrix& matrix = GetWorldMatrix();
	FVector forward(matrix.M[0][0], matrix.M[0][1], matrix.M[0][2]);
	forward.Normalize();
	return forward;
}

FVector USceneComponent::GetRightVector()
{
	const FMatrix& matrix = GetWorldMatrix();
	FVector right(matrix.M[1][0], matrix.M[1][1], matrix.M[1][2]);
	right.Normalize();
	return right;
}

FVector USceneComponent::GetUpVector()
{
	const FMatrix& matrix = GetWorldMatrix();
	FVector up(matrix.M[2][0], matrix.M[2][1], matrix.M[2][2]);
	up.Normalize();
	return up;
}

void USceneComponent::Move(const FVector& delta) {
	SetRelativeLocation(RelativeLocation + delta);
}

void USceneComponent::MoveLocal(const FVector& delta) {
	FVector forward = GetForwardVector();
	FVector right = GetRightVector();
	FVector up = GetUpVector();

	SetRelativeLocation(RelativeLocation
		+ forward * delta.X
		+ right * delta.Y
		+ up * delta.Z);
}

void USceneComponent::Rotate(float dx, float dy) {
	RelativeRotation.Z += dx;
	RelativeRotation.Y += dy;
	RelativeRotation.Y = Clamp(RelativeRotation.Y, -89.9f, 89.9f);

	RelativeRotation.X = 0.0f;

	SetRelativeRotation(RelativeRotation);   // keeps RelativeTransform in sync
}

//Temp
FMatrix USceneComponent::GetRelativeMatrixTemp() const
{
	return FTransform(RelativeLocation, RelativeRotation, RelativeScale3D).ToMatrix();
}