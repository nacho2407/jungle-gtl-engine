#include "GizmoComponent.h"
#include "Render/Mesh/MeshManager.h"

DEFINE_CLASS(UGizmoComponent, UPrimitiveComponent)
REGISTER_FACTORY(UGizmoComponent)

#include <cmath>
UGizmoComponent::UGizmoComponent()
{
	MeshData = &FMeshManager::GetTranslationGizmo();
	LocalExtents = FVector(1.5f, 1.5f, 1.5f);
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

void UGizmoComponent::TranslateTarget(float DragAmount)
{
	if (TargetComponent == nullptr) return;

	FVector ConstrainedDelta = GetVectorForAxis(SelectedAxis) * DragAmount;

	AddWorldOffset(ConstrainedDelta);
	TargetComponent->AddWorldOffset(ConstrainedDelta);
}

void UGizmoComponent::RotateTarget(float DragAmount)
{
	if (TargetComponent == nullptr) return;

	FMatrix CurMatrix = FMatrix::MakeRotationEuler(TargetComponent->GetRelativeRotation());

	FVector RotationAxis = GetVectorForAxis(SelectedAxis);

	FMatrix DeltaMatrix = FMatrix::MakeRotationAxis(RotationAxis, DragAmount);

	FMatrix NewMatrix = CurMatrix * DeltaMatrix;
	TargetComponent->SetRelativeRotation(NewMatrix.GetEuler());
}

void UGizmoComponent::ScaleTarget(float DragAmount)
{
	if (TargetComponent == nullptr) return;

	float ScaleDelta = DragAmount * ScaleSensitivity;

	FVector NewScale = TargetComponent->GetRelativeScale();
	switch (SelectedAxis)
	{
	case 0:
		NewScale.X += ScaleDelta;
		break;
	case 1:
		NewScale.Y += ScaleDelta;
		break;
	case 2:
		NewScale.Z += ScaleDelta;
		break;
	}

	TargetComponent->SetRelativeScale(NewScale);
}

void UGizmoComponent::SetTargetLocation(FVector NewLocation)
{
	if (TargetComponent == nullptr) return;

	TargetComponent->SetRelativeLocation(NewLocation);
	UpdateGizmoTransform();
}

void UGizmoComponent::SetTargetRotation(FVector NewRotation)
{
	if (TargetComponent == nullptr) return;

	TargetComponent->SetRelativeRotation(NewRotation);

	UpdateGizmoTransform();
}

void UGizmoComponent::SetTargetScale(FVector NewScale)
{
	if (TargetComponent == nullptr) return;

	FVector SafeScale = NewScale;
	if (SafeScale.X < 0.001f) SafeScale.X = 0.001f;
	if (SafeScale.Y < 0.001f) SafeScale.Y = 0.001f;
	if (SafeScale.Z < 0.001f) SafeScale.Z = 0.001f;

	TargetComponent->SetRelativeScale(SafeScale);
}

bool UGizmoComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	UPrimitiveComponent::RaycastMesh(Ray, OutHitResult);

	UpdateHoveredAxis(OutHitResult.FaceIndex);

	return OutHitResult.bHit;
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

void UGizmoComponent::SetTarget(USceneComponent* NewTargetComponent)
{
	if (NewTargetComponent == nullptr)
	{
		return;
	}

	TargetComponent = NewTargetComponent;

	SetWorldLocation(TargetComponent->GetWorldLocation());
	UpdateGizmoTransform();
	SetVisibility(true);
}

void UGizmoComponent::UpdateLinearDrag(const FRay& Ray)
{
	FVector AxisVector = GetVectorForAxis(SelectedAxis);

	FVector PlaneNormal = AxisVector.Cross(Ray.Direction);
	FVector ProjectDir = PlaneNormal.Cross(AxisVector);

	float Denom = Ray.Direction.Dot(ProjectDir);
	if (std::abs(Denom) < 1e-6f) return;

	float DistanceToPlane = (GetWorldLocation() - Ray.Origin).Dot(ProjectDir) / Denom;
	FVector CurrentIntersectionLocation = Ray.Origin + (Ray.Direction * DistanceToPlane);

	if (bIsFirstFrameOfDrag)
	{
		LastIntersectionLocation = CurrentIntersectionLocation;
		bIsFirstFrameOfDrag = false;
		return;
	}

	FVector FullDelta = CurrentIntersectionLocation - LastIntersectionLocation;

	float DragAmount = FullDelta.Dot(AxisVector);

	HandleDrag(DragAmount);

	LastIntersectionLocation = CurrentIntersectionLocation;
}

void UGizmoComponent::UpdateAngularDrag(const FRay& Ray)
{
	FVector AxisVector = GetVectorForAxis(SelectedAxis);
	FVector PlaneNormal = AxisVector;

	float Denom = Ray.Direction.Dot(PlaneNormal);
	if (std::abs(Denom) < 1e-6f) return;

	float DistanceToPlane = (GetWorldLocation() - Ray.Origin).Dot(PlaneNormal) / Denom;
	FVector CurrentIntersectionLocation = Ray.Origin + (Ray.Direction * DistanceToPlane);

	if (bIsFirstFrameOfDrag)
	{
		LastIntersectionLocation = CurrentIntersectionLocation;
		bIsFirstFrameOfDrag = false;
		return;
	}

	FVector CenterToLast = (LastIntersectionLocation - GetWorldLocation()).Normalized();
	FVector CenterToCurrent = (CurrentIntersectionLocation - GetWorldLocation()).Normalized();

	float DotProduct = Clamp(CenterToLast.Dot(CenterToCurrent), -1.0f, 1.0f);
	float AngleRadians = std::acos(DotProduct);

	FVector CrossProduct = CenterToLast.Cross(CenterToCurrent);
	float Sign = (CrossProduct.Dot(AxisVector) >= 0.0f) ? 1.0f : -1.0f;

	float DeltaAngle = Sign * AngleRadians;

	HandleDrag(DeltaAngle);

	LastIntersectionLocation = CurrentIntersectionLocation;
}

void UGizmoComponent::UpdateHoveredAxis(int Index)
{
	if (Index < 0)
	{
		if (IsHolding() == false) SelectedAxis = -1;
	}
	else
	{
		if (IsHolding() == false)
		{
			uint32 VertexIndex = MeshData->Indices[Index];
			SelectedAxis = MeshData->Vertices[VertexIndex].SubID;

		}
	}
}

void UGizmoComponent::UpdateDrag(const FRay& Ray)
{
	if (IsHolding() == false || IsActive() == false)
	{
		return;
	}
	if (SelectedAxis == -1 || TargetComponent == nullptr)
	{
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
	if (TargetComponent == nullptr) return;

	SetWorldLocation(TargetComponent->GetWorldLocation());

	switch (CurMode)
	{
	case EGizmoMode::Scale:
		SetRelativeRotation(TargetComponent->GetRelativeRotation());
		MeshData = &FMeshManager::Get().GetScaleGizmo();
		break;

	case EGizmoMode::Rotate:
		if (bIsWorldSpace)
		{
			SetRelativeRotation(FVector());
		}
		else
		{
			SetRelativeRotation(TargetComponent->GetRelativeRotation());
		}
		MeshData = &FMeshManager::Get().GetRotationGizmo();
		break;

	case EGizmoMode::Translate:
		if (bIsWorldSpace)
		{
			SetRelativeRotation(FVector());
		}
		else
		{
			SetRelativeRotation(TargetComponent->GetRelativeRotation());
		}
		MeshData = &FMeshManager::Get().GetTranslationGizmo();
		break;
	}
}

void UGizmoComponent::ApplyScreenSpaceScaling(const FVector& CameraLocation)
{
	float Distance = FVector::Distance(CameraLocation, GetWorldLocation());

	float NewScale = Distance * 0.1f;

	if (NewScale < 0.01f) NewScale = 0.01f;

	SetRelativeScale(FVector(NewScale, NewScale, NewScale));
}

void UGizmoComponent::SetWorldSpace(bool bWorldSpace)
{
	bIsWorldSpace = bWorldSpace;
	UpdateGizmoTransform();
}

bool UGizmoComponent::GetRenderCommand(const FMatrix& viewMatrix, const FMatrix& projMatrix, FRenderCommand& OutCommand)
{
	if (!MeshData || !bIsVisible) {
		return false;
	}

	return UPrimitiveComponent::GetRenderCommand(viewMatrix, projMatrix, OutCommand);

}

void UGizmoComponent::Deactivate()
{
	TargetComponent = nullptr;
	SetVisibility(false);
	SelectedAxis = -1;
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


