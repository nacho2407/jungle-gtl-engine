#include "GizmoComponent.h"
#include "GameFramework/AActor.h"
#include "Component/SceneComponent.h"
#include "Object/Object.h"
#include "Render/Mesh/MeshManager.h"
#include "Core/ResourceManager.h"

DEFINE_CLASS(UGizmoComponent, UPrimitiveComponent)
REGISTER_FACTORY(UGizmoComponent)

#include <cfloat>
#include <cmath>

UGizmoComponent::UGizmoComponent()
{
	GizmoMeshData = &FEditorMeshLibrary::GetTranslationGizmo();

	// Gizmo 전용 Material 생성
	Material = FResourceManager::Get().GetOrCreateMaterial("GizmoMaterial", "Shaders/EditorDebug/Gizmo.hlsl");
}

const FMeshData* UGizmoComponent::GetActiveMeshData() const
{
	return GizmoMeshData;
}

void UGizmoComponent::SetHolding(bool bHold)
{
	if (bIsHolding == bHold)
	{
		return;
	}

	if (bHold)
	{
		PendingSnapDelta = 0.0f;
	}

	bIsHolding = bHold;
}

void UGizmoComponent::UpdateWorldAABB() const
{
	WorldAABB.Reset();

	const FMatrix& WorldMatrix = GetWorldMatrix();

	const float NewEx = std::abs(WorldMatrix.M[0][0]) * LocalExtents.X +
		std::abs(WorldMatrix.M[1][0]) * LocalExtents.Y +
		std::abs(WorldMatrix.M[2][0]) * LocalExtents.Z;

	const float NewEy = std::abs(WorldMatrix.M[0][1]) * LocalExtents.X +
		std::abs(WorldMatrix.M[1][1]) * LocalExtents.Y +
		std::abs(WorldMatrix.M[2][1]) * LocalExtents.Z;

	const float NewEz = std::abs(WorldMatrix.M[0][2]) * LocalExtents.X +
		std::abs(WorldMatrix.M[1][2]) * LocalExtents.Y +
		std::abs(WorldMatrix.M[2][2]) * LocalExtents.Z;

	const FVector WorldCenter = GetWorldLocation();
	WorldAABB.Expand(WorldCenter - FVector(NewEx, NewEy, NewEz));
	WorldAABB.Expand(WorldCenter + FVector(NewEx, NewEy, NewEz));
}

bool UGizmoComponent::IntersectRayAxis(const FRay& Ray, FVector AxisEnd, float& OutRayT)
{
	FVector AxisStart = GetWorldLocation();
	FVector RayOrigin = Ray.Origin;
	FVector RayDirection = Ray.Direction;

	FVector AxisVector = AxisEnd - AxisStart;
	FVector DiffOrigin = RayOrigin - AxisStart;

	float RayDirDotRayDir = RayDirection.X * RayDirection.X + RayDirection.Y * RayDirection.Y + RayDirection.Z * RayDirection.Z;
	float RayDirDotAxis = RayDirection.X * AxisVector.X + RayDirection.Y * AxisVector.Y + RayDirection.Z * AxisVector.Z;
	float AxisDotAxis = AxisVector.X * AxisVector.X + AxisVector.Y * AxisVector.Y + AxisVector.Z * AxisVector.Z;
	float RayDirDotDiff = RayDirection.X * DiffOrigin.X + RayDirection.Y * DiffOrigin.Y + RayDirection.Z * DiffOrigin.Z;
	float AxisDotDiff = AxisVector.X * DiffOrigin.X + AxisVector.Y * DiffOrigin.Y + AxisVector.Z * DiffOrigin.Z;

	float Denominator = (RayDirDotRayDir * AxisDotAxis) - (RayDirDotAxis * RayDirDotAxis);

	float RayT;
	float AxisS;

	if (Denominator < 1e-6f)
	{
		RayT = 0.0f;
		AxisS = (AxisDotAxis > 0.0f) ? (AxisDotDiff / AxisDotAxis) : 0.0f;
	}
	else
	{
		RayT = (RayDirDotAxis * AxisDotDiff - AxisDotAxis * RayDirDotDiff) / Denominator;
		AxisS = (RayDirDotRayDir * AxisDotDiff - RayDirDotAxis * RayDirDotDiff) / Denominator;
	}

	if (RayT < 0.0f) RayT = 0.0f;

	if (AxisS < 0.0f) AxisS = 0.0f;
	else if (AxisS > 1.0f) AxisS = 1.0f;

	FVector ClosestPointOnRay = RayOrigin + (RayDirection * RayT);
	FVector ClosestPointOnAxis = AxisStart + (AxisVector * AxisS);

	FVector DistanceVector = ClosestPointOnRay - ClosestPointOnAxis;
	float DistanceSquared = (DistanceVector.X * DistanceVector.X) +
		(DistanceVector.Y * DistanceVector.Y) +
		(DistanceVector.Z * DistanceVector.Z);

	float ClickThresholdSquared = Radius * Radius;

	if (DistanceSquared < ClickThresholdSquared)
	{
		OutRayT = RayT;
		return true;
	}

	return false;
}

void UGizmoComponent::HandleDrag(float DragAmount)
{
	DragAmount = QuantizeDragAmount(DragAmount);
	if (std::abs(DragAmount) < 1e-6f)
	{
		return;
	}

	switch (CurMode)
	{
	case EGizmoMode::Translate:
		TranslateTarget(DragAmount);
		break;
	case EGizmoMode::Rotate:
		RotateTarget(DragAmount);
		break;
	case EGizmoMode::Scale:
		ScaleTarget(DragAmount);
		break;
	default:
		break;
	}

	UpdateGizmoTransform();
}

void UGizmoComponent::SetTranslateSnap(bool bEnabled, float Step)
{
	bTranslateSnapEnabled = bEnabled;
	if (Step > 0.0f)
	{
		TranslateSnapStep = Step;
	}
}

void UGizmoComponent::SetRotateSnap(bool bEnabled, float DegreesStep)
{
	bRotateSnapEnabled = bEnabled;
	if (DegreesStep > 0.0f)
	{
		RotateSnapStepDegrees = DegreesStep;
	}
}

void UGizmoComponent::SetScaleSnap(bool bEnabled, float Step)
{
	bScaleSnapEnabled = bEnabled;
	if (Step > 0.0f)
	{
		ScaleSnapStep = Step;
	}
}

float UGizmoComponent::QuantizeDragAmount(float DragAmount)
{
	float Step = 0.0f;
	switch (CurMode)
	{
	case EGizmoMode::Translate:
		if (!bTranslateSnapEnabled)
		{
			return DragAmount;
		}
		Step = TranslateSnapStep;
		break;
	case EGizmoMode::Rotate:
		if (!bRotateSnapEnabled)
		{
			return DragAmount;
		}
		Step = RotateSnapStepDegrees * MathUtil::DEG_TO_RAD;
		break;
	case EGizmoMode::Scale:
		if (!bScaleSnapEnabled)
		{
			return DragAmount;
		}
		Step = ScaleSnapStep;
		break;
	default:
		return DragAmount;
	}

	if (Step <= 1e-6f)
	{
		return DragAmount;
	}

	PendingSnapDelta += DragAmount;
	const float StepsFloat = PendingSnapDelta / Step;
	const float StepsWhole = (StepsFloat >= 0.0f) ? std::floor(StepsFloat) : std::ceil(StepsFloat);
	if (std::abs(StepsWhole) < 1e-6f)
	{
		return 0.0f;
	}

	const float SnappedDelta = StepsWhole * Step;
	PendingSnapDelta -= SnappedDelta;
	return SnappedDelta;
}

USceneComponent* UGizmoComponent::GetTargetSceneComponent() const
{
	if (TargetComponent)
	{
		return IsTargetComponentAlive() ? TargetComponent : nullptr;
	}
	return IsTargetActorAlive() ? TargetActor->GetRootComponent() : nullptr;
}

FVector UGizmoComponent::GetTargetLocation() const
{
	if (USceneComponent* SceneComponent = GetTargetSceneComponent())
	{
		return SceneComponent->GetWorldLocation();
	}
	return FVector::ZeroVector;
}

FVector UGizmoComponent::GetTargetRotation() const
{
	if (TargetComponent)
	{
		return IsTargetComponentAlive() ? TargetComponent->GetWorldTransform().GetRotation().Euler() : FVector::ZeroVector;
	}
	return IsTargetActorAlive() ? TargetActor->GetActorRotation() : FVector::ZeroVector;
}

FVector UGizmoComponent::GetTargetScale() const
{
	if (TargetComponent)
	{
		return IsTargetComponentAlive() ? TargetComponent->GetRelativeScale() : FVector::OneVector;
	}
	return IsTargetActorAlive() ? TargetActor->GetActorScale() : FVector::OneVector;
}

bool UGizmoComponent::IsTargetActorAlive() const
{
	if (!TargetActor || TargetActorUUID == 0)
	{
		return false;
	}

	if (!UObjectManager::Get().ContainsObject(TargetActor))
	{
		return false;
	}

	return TargetActor->GetUUID() == TargetActorUUID;
}

bool UGizmoComponent::IsTargetComponentAlive() const
{
	if (!TargetComponent || TargetComponentUUID == 0)
	{
		return false;
	}

	if (!UObjectManager::Get().ContainsObject(TargetComponent))
	{
		return false;
	}

	return TargetComponent->GetUUID() == TargetComponentUUID;
}

bool UGizmoComponent::HasTarget() const
{
	if (TargetComponent)
	{
		return IsTargetComponentAlive();
	}
	return IsTargetActorAlive();
}

void UGizmoComponent::TranslateTarget(float DragAmount)
{
	USceneComponent* TargetSceneComponent = GetTargetSceneComponent();
	if (!TargetSceneComponent) return;

	FVector ConstrainedDelta = GetVectorForAxis(SelectedAxis) * DragAmount;

	AddWorldOffset(ConstrainedDelta);

	if (IsTargetComponentAlive())
	{
		TargetComponent->AddWorldOffset(ConstrainedDelta);
	}
	else if (AllSelectedActors)
	{
		for (AActor* Actor : *AllSelectedActors)
		{
			if (Actor) Actor->AddActorWorldOffset(ConstrainedDelta);
		}
	}
	else
	{
		TargetActor->AddActorWorldOffset(ConstrainedDelta);
	}
}

void UGizmoComponent::RotateTarget(float DragAmount)
{
	USceneComponent* TargetSceneComponent = GetTargetSceneComponent();
	if (!TargetSceneComponent) return;

	FVector RotationAxis = GetVectorForAxis(SelectedAxis);
	RotationAxis.NormalizeSafe();
	FQuat DeltaQuat(RotationAxis, DragAmount);
	const FVector Pivot = GetTargetLocation();

	if (IsTargetComponentAlive())
	{
		FQuat CurrentQuat = TargetComponent->GetRelativeQuat();
		FQuat NewQuat = CurrentQuat * DeltaQuat;
		NewQuat.Normalize();
		TargetComponent->SetRelativeRotationQuat(NewQuat);
		return;
	}

	auto ApplyRotation = [&](AActor* Actor)
		{
			if (!Actor || !Actor->GetRootComponent()) return;
			if (Actor != TargetActor)
			{
				const FVector OffsetFromPivot = Actor->GetActorLocation() - Pivot;
				Actor->SetActorLocation(Pivot + DeltaQuat.RotateVector(OffsetFromPivot));
			}
			FQuat CurQuat = FQuat::MakeFromEuler(Actor->GetActorRotation());
			FQuat NewQuat = CurQuat * DeltaQuat;
			Actor->SetActorRotation(NewQuat.Euler());
		};

	if (AllSelectedActors)
	{
		for (AActor* Actor : *AllSelectedActors)
		{
			ApplyRotation(Actor);
		}
	}
	else
	{
		ApplyRotation(TargetActor);
	}
}

void UGizmoComponent::ScaleTarget(float DragAmount)
{
	USceneComponent* TargetSceneComponent = GetTargetSceneComponent();
	if (!TargetSceneComponent) return;

	float ScaleDelta = DragAmount * ScaleSensitivity;
	const FVector Pivot = GetTargetLocation();
	FVector ScaleAxis = GetVectorForAxis(SelectedAxis);
	ScaleAxis.NormalizeSafe();
	const float PivotScaleFactor = std::max(0.001f, 1.0f + ScaleDelta);

	if (IsTargetComponentAlive())
	{
		FVector NewScale = TargetComponent->GetRelativeScale();
		switch (SelectedAxis)
		{
		case 0: NewScale.X += ScaleDelta; break;
		case 1: NewScale.Y += ScaleDelta; break;
		case 2: NewScale.Z += ScaleDelta; break;
		default: break;
		}
		NewScale.X = std::max(0.001f, NewScale.X);
		NewScale.Y = std::max(0.001f, NewScale.Y);
		NewScale.Z = std::max(0.001f, NewScale.Z);
		TargetComponent->SetRelativeScale(NewScale);
		return;
	}

	auto ApplyScale = [&](AActor* Actor)
		{
			if (!Actor) return;
			if (AllSelectedActors && Actor != TargetActor && !ScaleAxis.IsNearlyZero())
			{
				const FVector OffsetFromPivot = Actor->GetActorLocation() - Pivot;
				const float AxisDistance = OffsetFromPivot.DotProduct(ScaleAxis);
				const FVector AxisOffset = ScaleAxis * AxisDistance;
				const FVector PerpendicularOffset = OffsetFromPivot - AxisOffset;
				Actor->SetActorLocation(Pivot + PerpendicularOffset + AxisOffset * PivotScaleFactor);
			}

			FVector NewScale = Actor->GetActorScale();
			switch (SelectedAxis)
			{
			case 0: NewScale.X += ScaleDelta; break;
			case 1: NewScale.Y += ScaleDelta; break;
			case 2: NewScale.Z += ScaleDelta; break;
			}
			NewScale.X = std::max(0.001f, NewScale.X);
			NewScale.Y = std::max(0.001f, NewScale.Y);
			NewScale.Z = std::max(0.001f, NewScale.Z);
			Actor->SetActorScale(NewScale);
		};

	if (AllSelectedActors)
	{
		for (AActor* Actor : *AllSelectedActors)
		{
			ApplyScale(Actor);
		}
	}
	else
	{
		ApplyScale(TargetActor);
	}
}

void UGizmoComponent::SetTargetLocation(FVector NewLocation)
{
	if (IsTargetComponentAlive())
	{
		TargetComponent->SetWorldLocation(NewLocation);
		UpdateGizmoTransform();
		return;
	}
	if (!TargetActor) return;

	TargetActor->SetActorLocation(NewLocation);
	UpdateGizmoTransform();
}

void UGizmoComponent::SetTargetRotation(FVector NewRotation)
{
	if (IsTargetComponentAlive())
	{
		TargetComponent->SetRelativeRotation(NewRotation);
		UpdateGizmoTransform();
		return;
	}
	if (!TargetActor) return;

	TargetActor->SetActorRotation(NewRotation);
	UpdateGizmoTransform();
}

void UGizmoComponent::SetTargetScale(FVector NewScale)
{
	FVector SafeScale = NewScale;
	if (SafeScale.X < 0.001f) SafeScale.X = 0.001f;
	if (SafeScale.Y < 0.001f) SafeScale.Y = 0.001f;
	if (SafeScale.Z < 0.001f) SafeScale.Z = 0.001f;

	if (IsTargetComponentAlive())
	{
		TargetComponent->SetRelativeScale(SafeScale);
		UpdateGizmoTransform();
		return;
	}
	if (!TargetActor) return;

	TargetActor->SetActorScale(SafeScale);
}

bool UGizmoComponent::HitTestMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	OutHitResult = {};

	const FMeshData* MeshData = GetActiveMeshData();
	if (!MeshData || MeshData->Indices.empty())
	{
		return false;
	}

	const FMatrix InvWorld = GetWorldMatrix().GetInverse();
	FVector LocalOrigin = InvWorld.TransformPosition(Ray.Origin);
	FVector LocalDirection = InvWorld.TransformVector(Ray.Direction);
	LocalDirection.NormalizeSafe();

	bool bHit = false;
	float ClosestT = FLT_MAX;

	for (size_t i = 0; i + 2 < MeshData->Indices.size(); i += 3)
	{
		const FVector& V0 = MeshData->Vertices[MeshData->Indices[i]].Position;
		const FVector& V1 = MeshData->Vertices[MeshData->Indices[i + 1]].Position;
		const FVector& V2 = MeshData->Vertices[MeshData->Indices[i + 2]].Position;

		float HitT = 0.0f;
		if (IntersectTriangle(LocalOrigin, LocalDirection, V0, V1, V2, HitT) && HitT < ClosestT)
		{
			ClosestT = HitT;
			bHit = true;
			OutHitResult.FaceIndex = static_cast<int32>(i);
		}
	}

	OutHitResult.bHit = bHit;
	if (!bHit)
	{
		return false;
	}

	const FVector LocalHitPoint = LocalOrigin + (LocalDirection * ClosestT);
	const FVector WorldHitPoint = GetWorldMatrix().TransformPosition(LocalHitPoint);
	OutHitResult.Distance = FVector::Distance(Ray.Origin, WorldHitPoint);
	OutHitResult.Location = WorldHitPoint;
	OutHitResult.HitComponent = this;

	return OutHitResult.bHit;
}

bool UGizmoComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	if (!HitTestMesh(Ray, OutHitResult))
	{
		UpdateHoveredAxis(-1);
		return false;
	}

	UpdateHoveredAxis(OutHitResult.FaceIndex);
	return true;
}


FVector UGizmoComponent::GetVectorForAxis(int32 Axis)
{
	switch (Axis)
	{
	case 0:
		return GetForwardVector();
	case 1:
		return GetRightVector();
	case 2:
		return GetUpVector();
	default:
		return FVector(0.f, 0.f, 0.f);
	}
}

void UGizmoComponent::SetTarget(AActor* NewTarget)
{
	DragEnd();

	if (!NewTarget || !NewTarget->GetRootComponent())
	{
		Deactivate();
		return;
	}

	TargetActor = NewTarget;
	TargetComponent = nullptr;
	TargetActorUUID = TargetActor->GetUUID();
	TargetComponentUUID = 0;

	SetWorldLocation(TargetActor->GetActorLocation());
	UpdateGizmoTransform();
	SetVisibility(true);
}

void UGizmoComponent::SetTargetComponent(USceneComponent* NewTarget)
{
	DragEnd();

	if (!NewTarget)
	{
		Deactivate();
		return;
	}

	TargetActor = NewTarget->GetOwner();
	TargetComponent = NewTarget;
	TargetActorUUID = TargetActor ? TargetActor->GetUUID() : 0;
	TargetComponentUUID = TargetComponent->GetUUID();
	AllSelectedActors = nullptr;

	SetWorldLocation(TargetComponent->GetWorldLocation());
	UpdateGizmoTransform();
	SetVisibility(true);
}

void UGizmoComponent::UpdateLinearDrag(const FRay& Ray)
{
	FVector AxisVector = GetVectorForAxis(SelectedAxis);

	FVector ViewDir = (GetWorldLocation() - Ray.Origin);
	ViewDir.NormalizeSafe();

	// 고정된 뷰 벡터와 축을 외적하여 마우스를 아무리 움직여도 뒤집히지 않는 고정 평면을 만든다.
	FVector PlaneNormal = AxisVector.CrossProduct(ViewDir);

	// 시선과 기즈모 축이 완벽하게 일직선이 되어 외적 결과가 영벡터가 되는 특수 경우 예외 처리
	if (PlaneNormal.SizeSquared() < 1e-6f)
	{
		PlaneNormal = AxisVector.CrossProduct(FVector::UpVector);
	}
	PlaneNormal.NormalizeSafe();

	FVector ProjectDir = PlaneNormal.CrossProduct(AxisVector);

	float Denom = Ray.Direction.DotProduct(ProjectDir);
	if (std::abs(Denom) < 1e-6f) return;

	float DistanceToPlane = (GetWorldLocation() - Ray.Origin).DotProduct(ProjectDir) / Denom;
	FVector CurrentIntersectionLocation = Ray.Origin + (Ray.Direction * DistanceToPlane);

	if (bIsFirstFrameOfDrag)
	{
		LastIntersectionLocation = CurrentIntersectionLocation;
		bIsFirstFrameOfDrag = false;
		return;
	}

	FVector FullDelta = CurrentIntersectionLocation - LastIntersectionLocation;

	float DragAmount = FullDelta.DotProduct(AxisVector);

	HandleDrag(DragAmount);

	LastIntersectionLocation = CurrentIntersectionLocation;
}

void UGizmoComponent::UpdateAngularDrag(const FRay& Ray)
{
	FVector AxisVector = GetVectorForAxis(SelectedAxis);
	FVector PlaneNormal = AxisVector;

	float Denom = Ray.Direction.DotProduct(PlaneNormal);
	if (std::abs(Denom) < 1e-6f) return;

	float DistanceToPlane = (GetWorldLocation() - Ray.Origin).DotProduct(PlaneNormal) / Denom;
	FVector CurrentIntersectionLocation = Ray.Origin + (Ray.Direction * DistanceToPlane);

	if (bIsFirstFrameOfDrag)
	{
		LastIntersectionLocation = CurrentIntersectionLocation;
		bIsFirstFrameOfDrag = false;
		return;
	}

	FVector CenterToLast = (LastIntersectionLocation - GetWorldLocation()).Normalized();
	FVector CenterToCurrent = (CurrentIntersectionLocation - GetWorldLocation()).Normalized();

	float DotProduct = MathUtil::Clamp(CenterToLast.DotProduct(CenterToCurrent), -1.0f, 1.0f);
	float AngleRadians = std::acos(DotProduct);

	FVector CrossProduct = CenterToLast.CrossProduct(CenterToCurrent);
	float Sign = (CrossProduct.DotProduct(AxisVector) >= 0.0f) ? 1.0f : -1.0f;

	float DeltaAngle = Sign * AngleRadians;

	HandleDrag(DeltaAngle);

	LastIntersectionLocation = CurrentIntersectionLocation;
}

void UGizmoComponent::UpdateHoveredAxis(int Index)
{
	if (IsHolding() || IsPressedOnHandle())
	{
		return;
	}

	// 조작 중이 아닐 때만 마우스 Raycast 결과에 따라 축을 갱신합니다.
	if (Index < 0)
	{
		SelectedAxis = -1;
	}
	else
	{
		const FMeshData* MeshData = GetActiveMeshData();
		if (!MeshData)
		{
			SelectedAxis = -1;
			return;
		}

		uint32 VertexIndex = MeshData->Indices[Index];
		SelectedAxis = MeshData->Vertices[VertexIndex].SubID;
	}
}

void UGizmoComponent::UpdateDrag(const FRay& Ray)
{
	if (IsHolding() == false || IsActive() == false)
	{
		return;
	}

	if (SelectedAxis == -1 || !HasTarget())
	{
		if (!HasTarget())
		{
			Deactivate();
		}
		return;
	}

	if (CurMode == EGizmoMode::Rotate)
	{
		UpdateAngularDrag(Ray);
	}

	else
	{
		UpdateLinearDrag(Ray);
	}
}

void UGizmoComponent::DragEnd()
{
	bIsFirstFrameOfDrag = true;
	SetHolding(false);
	SetPressedOnHandle(false);
	SelectedAxis = -1;
	PendingSnapDelta = 0.0f;
}

void UGizmoComponent::SetNextMode()
{
	EGizmoMode NextMode = static_cast<EGizmoMode>((static_cast<int>(CurMode) + 1) % EGizmoMode::End);
	UpdateGizmoMode(NextMode);
}

void UGizmoComponent::UpdateGizmoMode(EGizmoMode NewMode)
{
	CurMode = NewMode;
	UpdateGizmoTransform();
}

void UGizmoComponent::UpdateGizmoTransform()
{
	USceneComponent* TargetSceneComponent = GetTargetSceneComponent();
	if (!TargetSceneComponent)
	{
		Deactivate();
		return;
	}

	SetWorldLocation(TargetSceneComponent->GetWorldLocation());

	FVector TargetRot = GetTargetRotation();

	switch (CurMode)
	{
	case EGizmoMode::Scale:
		SetRelativeRotation(TargetRot);
		GizmoMeshData = &FEditorMeshLibrary::Get().GetScaleGizmo();
		break;

	case EGizmoMode::Rotate:
		SetRelativeRotation(bIsWorldSpace ? FVector() : TargetRot);
		GizmoMeshData = &FEditorMeshLibrary::Get().GetRotationGizmo();
		break;

	case EGizmoMode::Translate:
		SetRelativeRotation(bIsWorldSpace ? FVector() : TargetRot);
		GizmoMeshData = &FEditorMeshLibrary::Get().GetTranslationGizmo();
		break;
	}
}

void UGizmoComponent::ApplyScreenSpaceScaling(const FVector& CameraLocation)
{
	float Distance = FVector::Distance(CameraLocation, GetWorldLocation());

	float NewScale = Distance * 0.17f;

	if (NewScale < 0.01f) NewScale = 0.01f;

	SetRelativeScale(FVector(NewScale, NewScale, NewScale));
}

void UGizmoComponent::ApplyScreenSpaceScalingOrtho(float OrthoHeight)
{
	float NewScale = OrthoHeight * 0.15f;
	if (NewScale < 0.01f) NewScale = 0.01f;
	SetRelativeScale(FVector(NewScale, NewScale, NewScale));
}

void UGizmoComponent::SetWorldSpace(bool bWorldSpace)
{
	bIsWorldSpace = bWorldSpace;
	UpdateGizmoTransform();
}


void UGizmoComponent::Deactivate()
{
	DragEnd();
	TargetActor = nullptr;
	TargetComponent = nullptr;
	TargetActorUUID = 0;
	TargetComponentUUID = 0;
	AllSelectedActors = nullptr;
	SetVisibility(false);
}

EPrimitiveType UGizmoComponent::GetPrimitiveType() const
{
	EPrimitiveType CurPrimitiveType = EPrimitiveType::EPT_TransGizmo;
	switch (CurMode)
	{
	case EGizmoMode::Translate:
		break;
	case EGizmoMode::Rotate:
		CurPrimitiveType = EPrimitiveType::EPT_RotGizmo;
		break;
	case EGizmoMode::Scale:
		CurPrimitiveType = EPrimitiveType::EPT_ScaleGizmo;
		break;
	}
	return CurPrimitiveType;
}
