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
	FVector axisStart = GetWorldLocation();
	FVector rayOrigin = Ray.Origin;
	FVector rayDirection = Ray.Direction;

	FVector axisVector = AxisEnd - axisStart;
	FVector diffOrigin = rayOrigin - axisStart;

	float RayDirDotRayDir = rayDirection.X * rayDirection.X + rayDirection.Y * rayDirection.Y + rayDirection.Z * rayDirection.Z;
	float RayDirDotAxis = rayDirection.X * axisVector.X + rayDirection.Y * axisVector.Y + rayDirection.Z * axisVector.Z;
	float AxisDotAxis = axisVector.X * axisVector.X + axisVector.Y * axisVector.Y + axisVector.Z * axisVector.Z;
	float RayDirDotDiff = rayDirection.X * diffOrigin.X + rayDirection.Y * diffOrigin.Y + rayDirection.Z * diffOrigin.Z;
	float AxisDotDiff = axisVector.X * diffOrigin.X + axisVector.Y * diffOrigin.Y + axisVector.Z * diffOrigin.Z;

	float Denominator = (RayDirDotRayDir * AxisDotAxis) - (RayDirDotAxis * RayDirDotAxis);

	float rayT;
	float axisS;

	if (Denominator < 1e-6f)
	{
		rayT = 0.0f;
		axisS = (AxisDotAxis > 0.0f) ? (AxisDotDiff / AxisDotAxis) : 0.0f;
	}
	else
	{
		rayT = (RayDirDotAxis * AxisDotDiff - AxisDotAxis * RayDirDotDiff) / Denominator;
		axisS = (RayDirDotRayDir * AxisDotDiff - RayDirDotAxis * RayDirDotDiff) / Denominator;
	}

	if (rayT < 0.0f) rayT = 0.0f;

	if (axisS < 0.0f) axisS = 0.0f;
	else if (axisS > 1.0f) axisS = 1.0f;

	FVector closestPointOnRay = rayOrigin + (rayDirection * rayT);
	FVector closestPointOnAxis = axisStart + (axisVector * axisS);

	FVector distanceVector = closestPointOnRay - closestPointOnAxis;
	float distanceSquared = (distanceVector.X * distanceVector.X) +
		(distanceVector.Y * distanceVector.Y) +
		(distanceVector.Z * distanceVector.Z);

	float clickThresholdSquared = Radius * Radius;

	if (distanceSquared < clickThresholdSquared)
	{
		OutRayT = rayT;
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

	FVector constrainedDelta = GetVectorForAxis(SelectedAxis) * DragAmount;

	AddWorldOffset(constrainedDelta);
	TargetComponent->AddWorldOffset(constrainedDelta);
}

void UGizmoComponent::RotateTarget(float DragAmount)
{
	if (TargetComponent == nullptr) return;

	FMatrix curMatrix = FMatrix::MakeRotationEuler(TargetComponent->RelativeRotation);

	FVector rotationAxis = GetVectorForAxis(SelectedAxis);

	FMatrix deltaMatrix = FMatrix::MakeRotationAxis(rotationAxis, DragAmount);

	FMatrix NewMatrix = curMatrix * deltaMatrix;
	TargetComponent->SetRelativeRotation(NewMatrix.GetEuler());
}

void UGizmoComponent::ScaleTarget(float DragAmount)
{
	if (TargetComponent == nullptr) return;

	float scaleDelta = DragAmount * ScaleSensitivity;

	FVector NewScale = TargetComponent->RelativeScale3D;
	switch (SelectedAxis)
	{
	case 0:
		NewScale.X += scaleDelta;
		break;
	case 1:
		NewScale.Y += scaleDelta;
		break;
	case 2:
		NewScale.Z += scaleDelta;
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


FVector UGizmoComponent::GetVectorForAxis(int32 axis)
{
	switch (axis)
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
	FVector axisVector = GetVectorForAxis(SelectedAxis);

	FVector planeNormal = axisVector.Cross(Ray.Direction);
	FVector projectDir = planeNormal.Cross(axisVector);

	float Denom = Ray.Direction.Dot(projectDir);
	if (std::abs(Denom) < 1e-6f) return;

	float DistanceToPlane = (GetWorldLocation() - Ray.Origin).Dot(projectDir) / Denom;
	FVector CurrentIntersectionLocation = Ray.Origin + (Ray.Direction * DistanceToPlane);

	if (bIsFirstFrameOfDrag)
	{
		LastIntersectionLocation = CurrentIntersectionLocation;
		bIsFirstFrameOfDrag = false;
		return;
	}

	FVector fullDelta = CurrentIntersectionLocation - LastIntersectionLocation;

	float DragAmount = fullDelta.Dot(axisVector);

	HandleDrag(DragAmount);

	LastIntersectionLocation = CurrentIntersectionLocation;
}

void UGizmoComponent::UpdateAngularDrag(const FRay& Ray)
{
	FVector axisVector = GetVectorForAxis(SelectedAxis);
	FVector planeNormal = axisVector;

	float Denom = Ray.Direction.Dot(planeNormal);
	if (std::abs(Denom) < 1e-6f) return;

	float DistanceToPlane = (GetWorldLocation() - Ray.Origin).Dot(planeNormal) / Denom;
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
	float Sign = (CrossProduct.Dot(axisVector) >= 0.0f) ? 1.0f : -1.0f;

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
	EGizmoMode newMode = static_cast<EGizmoMode>((static_cast<int>(CurMode) + 1) % EGizmoMode::End);
	UpdateGizmoMode(newMode);
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
		SetRelativeRotation(TargetComponent->RelativeRotation);
		MeshData = &FMeshManager::Get().GetScaleGizmo();
		break;

	case EGizmoMode::Rotate:
		if (bIsWorldSpace)
		{
			SetRelativeRotation(FVector());
		}
		else
		{
			SetRelativeRotation(TargetComponent->RelativeRotation);
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
			SetRelativeRotation(TargetComponent->RelativeRotation);
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

bool UGizmoComponent::GetRenderCommand(FRenderCommand& OutCommand)
{
	if (!MeshData || !bIsVisible) {
		return false;
	}

	return UPrimitiveComponent::GetRenderCommand(OutCommand);

}

void UGizmoComponent::Deactivate()
{
	TargetComponent = nullptr;
	SetVisibility(false);
	SelectedAxis = -1;
}

EPrimitiveType UGizmoComponent::GetPrimitiveType() const
{
	EPrimitiveType curPrimitiveType = EPrimitiveType::EPT_TransGizmo;
	switch (CurMode)
	{
	case EGizmoMode::Translate:
		break;
	case EGizmoMode::Rotate:
		curPrimitiveType = EPrimitiveType::EPT_RotGizmo;
		break;
	case EGizmoMode::Scale:
		curPrimitiveType = EPrimitiveType::EPT_ScaleGizmo;
		break;
	}
	return curPrimitiveType;
}


