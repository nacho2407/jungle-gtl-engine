#include "GizmoComponent.h"
#include "GameFramework/AActor.h"
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
	Material = FResourceManager::Get().GetOrCreateMaterial("GizmoMaterial", "Shaders/Gizmo.hlsl");
}

const FMeshData* UGizmoComponent::GetActiveMeshData() const
{
	return GizmoMeshData;
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
	if (!TargetActor || !TargetActor->GetRootComponent()) return;

	FVector ConstrainedDelta = GetVectorForAxis(SelectedAxis) * DragAmount;

	AddWorldOffset(ConstrainedDelta);

	if (AllSelectedActors)
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
	if (!TargetActor || !TargetActor->GetRootComponent()) return;

	FVector RotationAxis = GetVectorForAxis(SelectedAxis);
	FQuat DeltaQuat(RotationAxis, DragAmount);

	auto ApplyRotation = [&](AActor* Actor)
		{
			if (!Actor || !Actor->GetRootComponent()) return;
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
	if (!TargetActor || !TargetActor->GetRootComponent()) return;

	float ScaleDelta = DragAmount * ScaleSensitivity;

	auto ApplyScale = [&](AActor* Actor)
		{
			if (!Actor) return;
			FVector NewScale = Actor->GetActorScale();
			switch (SelectedAxis)
			{
			case 0: NewScale.X += ScaleDelta; break;
			case 1: NewScale.Y += ScaleDelta; break;
			case 2: NewScale.Z += ScaleDelta; break;
			}
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
	if (!TargetActor) return;

	TargetActor->SetActorLocation(NewLocation);
	UpdateGizmoTransform();
}

void UGizmoComponent::SetTargetRotation(FVector NewRotation)
{
	if (!TargetActor) return;

	TargetActor->SetActorRotation(NewRotation);
	UpdateGizmoTransform();
}

void UGizmoComponent::SetTargetScale(FVector NewScale)
{
	if (!TargetActor) return;

	FVector SafeScale = NewScale;
	if (SafeScale.X < 0.001f) SafeScale.X = 0.001f;
	if (SafeScale.Y < 0.001f) SafeScale.Y = 0.001f;
	if (SafeScale.Z < 0.001f) SafeScale.Z = 0.001f;

	TargetActor->SetActorScale(SafeScale);
}

bool UGizmoComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
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
		UpdateHoveredAxis(-1);
		return false;
	}

	const FVector LocalHitPoint = LocalOrigin + (LocalDirection * ClosestT);
	const FVector WorldHitPoint = GetWorldMatrix().TransformPosition(LocalHitPoint);
	OutHitResult.Distance = FVector::Distance(Ray.Origin, WorldHitPoint);
	OutHitResult.Location = WorldHitPoint;
	OutHitResult.HitComponent = this;

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

void UGizmoComponent::SetTarget(AActor* NewTarget)
{
	if (!NewTarget || !NewTarget->GetRootComponent())
	{
		return;
	}

	TargetActor = NewTarget;

	SetWorldLocation(TargetActor->GetActorLocation());
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

	if (SelectedAxis == -1 || TargetActor == nullptr)
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
	SetPressedOnHandle(false);
	SelectedAxis = -1;
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
	if (!TargetActor || !TargetActor->GetRootComponent()) return;

	SetWorldLocation(TargetActor->GetActorLocation());

	FVector ActorRot = TargetActor->GetActorRotation();

	switch (CurMode)
	{
	case EGizmoMode::Scale:
		SetRelativeRotation(ActorRot);
		GizmoMeshData = &FEditorMeshLibrary::Get().GetScaleGizmo();
		break;

	case EGizmoMode::Rotate:
		SetRelativeRotation(bIsWorldSpace ? FVector() : ActorRot);
		GizmoMeshData = &FEditorMeshLibrary::Get().GetRotationGizmo();
		break;

	case EGizmoMode::Translate:
		SetRelativeRotation(bIsWorldSpace ? FVector() : ActorRot);
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
	TargetActor = nullptr;
	AllSelectedActors = nullptr;
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