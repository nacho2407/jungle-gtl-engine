#include "SceneComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(USceneComponent, UActorComponent)
REGISTER_FACTORY(USceneComponent)

// 깊은 복사를 하더라도 SceneComponent의 부모-자식 관계는 초기화됩니다.
// Actor에서 Duplicate()할 때 이 구조를 Fix-up(복원)해야 합니다.
USceneComponent* USceneComponent::Duplicate()
{
    USceneComponent* NewComp = UObjectManager::Get().CreateObject<USceneComponent>();
    NewComp->SetActive(this->IsActive());
    NewComp->SetOwner(nullptr);

    NewComp->RelativeLocation = this->RelativeLocation;
    NewComp->RelativeRotation = this->RelativeRotation;
    NewComp->RelativeScale3D  = this->RelativeScale3D;

    // 캐시된 데이터는 복사하지만, 새 부모에 붙을 때 다시 계산되도록 Dirty 플래그를 켭니다.
    NewComp->CachedWorldMatrix = this->CachedWorldMatrix;
    NewComp->CachedWorldTransform = this->CachedWorldTransform;
    NewComp->bTransformDirty = true;

    NewComp->ParentComponent = nullptr;
    NewComp->ChildComponents.clear();

	NewComp->DuplicateSubObjects();

    return NewComp;
}

USceneComponent::USceneComponent()
{
	CachedWorldMatrix = FMatrix::Identity;
	CachedWorldTransform = FTransform::Identity;
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

void USceneComponent::AttachToComponent(USceneComponent* InParent)
{
	if (InParent == nullptr || InParent == this)
	{
		return;
	}

	SetParent(InParent);
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
		if (!ParentComponent->ContainsChild(this))
		{
			ParentComponent->ChildComponents.push_back(this);
		}
	}

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

	auto Iter = std::find(ChildComponents.begin(), ChildComponents.end(), Child);
	if (Iter != ChildComponents.end())
	{
		if ((*Iter)->ParentComponent == this)
		{
			(*Iter)->ParentComponent = nullptr;
			(*Iter)->MarkTransformDirty();
		}

		ChildComponents.erase(Iter);
	}
}

bool USceneComponent::ContainsChild(const USceneComponent* Child) const
{
	if (Child == nullptr)
	{
		return false;
	}

	return std::find(ChildComponents.begin(), ChildComponents.end(), Child) != ChildComponents.end();
}

void USceneComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Location", EPropertyType::Vec3, &RelativeLocation, 0.0f, 0.0f, 0.1f });
	OutProps.push_back({ "Rotation", EPropertyType::Vec3, &RelativeRotation, 0.0f, 0.0f, 0.1f });
	OutProps.push_back({ "Scale", EPropertyType::Vec3, &RelativeScale3D, 0.0f, 0.0f, 0.1f });
}

void USceneComponent::PostEditProperty(const char* PropertyName)
{
	UActorComponent::PostEditProperty(PropertyName);
	MarkTransformDirty();
}

FRotator USceneComponent::GetRelativeRotator() const
{
	FRotator Rot = FRotator::MakeFromEuler(RelativeRotation);
	Rot.Normalize();
	return Rot;
}

FQuat USceneComponent::GetRelativeQuat() const
{
	return GetRelativeRotator().Quaternion();
}

void USceneComponent::SetRelativeRotationRotator(const FRotator& NewRotation)
{
	FRotator Normalized = NewRotation;
	Normalized.Normalize();

	// 에디터/카메라 용도에서 roll drift 방지
	if (MathUtil::Abs(Normalized.Roll) < 1e-6f)
	{
		Normalized.Roll = 0.0f;
	}

	RelativeRotation = Normalized.Euler();
	MarkTransformDirty();
}

void USceneComponent::SetRelativeRotationQuat(const FQuat& NewRotationQuat)
{
	FQuat NormalizedQuat = NewRotationQuat;
	NormalizedQuat.Normalize();

	FRotator Rot = NormalizedQuat.Rotator();
	Rot.Normalize();

	if (MathUtil::Abs(Rot.Roll) < 1e-6f)
	{
		Rot.Roll = 0.0f;
	}

	RelativeRotation = Rot.Euler();
	MarkTransformDirty();
}

void USceneComponent::SetRelativeLocation(const FVector& NewLocation)
{
	RelativeLocation = NewLocation;
	MarkTransformDirty();
}

void USceneComponent::SetRelativeRotation(const FVector& NewRotation)
{
	// 기존 인터페이스 유지:
	// 외부에서 Euler 벡터를 넣으면 내부에서 rotator normalize 후 다시 저장
	FRotator Rot = FRotator::MakeFromEuler(NewRotation);
	Rot.Normalize();
	RelativeRotation = Rot.Euler();
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
		if (Child)
		{
			Child->MarkTransformDirty();
		}
	}
}

FTransform USceneComponent::GetRelativeTransform() const
{
	return FTransform(GetRelativeRotator(), RelativeLocation, RelativeScale3D);
}

FMatrix USceneComponent::GetRelativeMatrix() const
{
	return GetRelativeTransform().ToMatrixWithScale();
}

void USceneComponent::UpdateWorldMatrix() const
{
	if (!bTransformDirty)
	{
		return;
	}

	const FTransform RelativeTransform = GetRelativeTransform();

	if (ParentComponent != nullptr)
	{
		CachedWorldTransform = RelativeTransform * ParentComponent->GetWorldTransform();
	}
	else
	{
		CachedWorldTransform = RelativeTransform;
	}

	CachedWorldMatrix = CachedWorldTransform.ToMatrixWithScale();
	bTransformDirty = false;
}

const FMatrix& USceneComponent::GetWorldMatrix() const
{
	if (bTransformDirty)
	{
		UpdateWorldMatrix();
	}

	return CachedWorldMatrix;
}

FTransform USceneComponent::GetWorldTransform() const
{
	if (bTransformDirty)
	{
		UpdateWorldMatrix();
	}

	return CachedWorldTransform;
}

void USceneComponent::SetWorldLocation(FVector NewWorldLocation)
{
	if (ParentComponent != nullptr)
	{
		const FTransform ParentWorldInverse = ParentComponent->GetWorldTransform().Inverse();
		const FVector NewRelativeLocation = ParentWorldInverse.TransformPosition(NewWorldLocation);
		SetRelativeLocation(NewRelativeLocation);
	}
	else
	{
		SetRelativeLocation(NewWorldLocation);
	}
}

FVector USceneComponent::GetWorldLocation() const
{
	return GetWorldTransform().GetTranslation();
}

FVector USceneComponent::GetWorldScale() const
{
	return GetWorldTransform().GetScale3D();
}

FVector USceneComponent::GetForwardVector() const
{
	return GetWorldTransform().GetUnitAxis(EAxis::X);
}

FVector USceneComponent::GetRightVector() const
{
	return GetWorldTransform().GetUnitAxis(EAxis::Y);
}

FVector USceneComponent::GetUpVector() const
{
	return GetWorldTransform().GetUnitAxis(EAxis::Z);
}

void USceneComponent::Move(const FVector& Delta)
{
	SetRelativeLocation(RelativeLocation + Delta);
}

void USceneComponent::MoveLocal(const FVector& Delta)
{
	// 핵심 수정:
	// 예전 코드는 world 축(GetForwardVector 등)을 구해놓고 relative location에 더해서
	// 부모가 있을 때 local/world가 섞였음.
	//
	// 여기서는 "내 relative rotation" 기준 local delta를 만든 뒤
	// relative location에 더함.
	const FQuat LocalQuat = GetRelativeQuat();

	const FVector LocalOffset =
		LocalQuat.GetAxisX() * Delta.X +
		LocalQuat.GetAxisY() * Delta.Y +
		LocalQuat.GetAxisZ() * Delta.Z;

	SetRelativeLocation(RelativeLocation + LocalOffset);
}

void USceneComponent::AddWorldOffset(const FVector& WorldDelta)
{
	if (ParentComponent == nullptr)
	{
		SetRelativeLocation(RelativeLocation + WorldDelta);
		return;
	}

	const FTransform ParentWorldInverse = ParentComponent->GetWorldTransform().Inverse();
	const FVector LocalDelta = ParentWorldInverse.TransformVector(WorldDelta);
	SetRelativeLocation(RelativeLocation + LocalDelta);
}

void USceneComponent::AddRelativeYaw(float DeltaYawDegrees)
{
	if (MathUtil::Abs(DeltaYawDegrees) < 1e-6f)
	{
		return;
	}

	// Yaw는 "부모 기준 Up(Z)" 축으로 회전시키는 게 local 계층에서도 가장 일관적임.
	// 부모가 없으면 사실상 world up 기준과 동일.
	const FVector ParentUpAxis = FVector(0.0f, 0.0f, 1.0f);

	FQuat CurrentQuat = GetRelativeQuat();
	FQuat DeltaQuat(ParentUpAxis, MathUtil::DegreesToRadians(DeltaYawDegrees));

	FQuat ResultQuat = DeltaQuat * CurrentQuat;
	ResultQuat.Normalize();

	SetRelativeRotationQuat(ResultQuat);
}

void USceneComponent::AddRelativePitch(float DeltaPitchDegrees)
{
	if (MathUtil::Abs(DeltaPitchDegrees) < 1e-6f)
	{
		return;
	}

	// pitch는 현재 local right 축 기준으로 회전
	FQuat CurrentQuat = GetRelativeQuat();
	const FVector LocalRightAxis = CurrentQuat.GetAxisY();

	FQuat DeltaQuat(LocalRightAxis, MathUtil::DegreesToRadians(DeltaPitchDegrees));
	FQuat ResultQuat = DeltaQuat * CurrentQuat;
	ResultQuat.Normalize();

	FRotator ResultRot = ResultQuat.Rotator();
	ResultRot.Pitch = MathUtil::Clamp(ResultRot.Pitch, -89.9f, 89.9f);
	ResultRot.Roll = 0.0f;
	ResultRot.Normalize();

	SetRelativeRotationRotator(ResultRot);
}

void USceneComponent::Rotate(float DeltaYaw, float DeltaPitch)
{
	// 기존 인터페이스 유지하면서 내부는 quat 기반 처리
	if (MathUtil::Abs(DeltaYaw) > 1e-6f)
	{
		AddRelativeYaw(DeltaYaw);
	}

	if (MathUtil::Abs(DeltaPitch) > 1e-6f)
	{
		AddRelativePitch(DeltaPitch);
	}
}