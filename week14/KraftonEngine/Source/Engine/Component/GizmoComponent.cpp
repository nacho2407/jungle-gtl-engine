#include "GizmoComponent.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "Math/Quat.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Resource/ConstantBufferPool.h"
#include "Collision/RayUtils.h"

IMPLEMENT_CLASS(UGizmoComponent, UPrimitiveComponent)

#include <cmath>
UGizmoComponent::UGizmoComponent()
{
	MeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::TransGizmo);
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
	FQuat DeltaQuat = FQuat::FromAxisAngle(RotationAxis, DragAmount);

	auto ApplyRotation = [&](AActor* Actor)
		{
			if (!Actor || !Actor->GetRootComponent()) return;
			FQuat CurQuat = Actor->GetActorRotation().ToQuaternion();
			FQuat NewQuat = DeltaQuat * CurQuat;
			Actor->SetActorRotation(NewQuat.ToRotator());
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

void UGizmoComponent::SetTargetRotation(FRotator NewRotation)
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

bool UGizmoComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	if (!MeshData || MeshData->Indices.empty()) return false;

	bool bHit = FRayUtils::RaycastTriangles(
		Ray, CachedWorldMatrix,
		&MeshData->Vertices[0].Position,
		sizeof(FVertex),
		MeshData->Indices,
		OutHitResult);

	if (bHit)
	{
		OutHitResult.HitComponent = this;
	}

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
			uint32 HitAxis = MeshData->Vertices[VertexIndex].SubID;

			// 마스크에 의해 숨겨진 축은 선택 불가
			if (AxisMask & (1u << HitAxis))
			{
				SelectedAxis = HitAxis;
			}
			else
			{
				SelectedAxis = -1;
			}
		}
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

	FRotator ActorRot = TargetActor->GetActorRotation();

	switch (CurMode)
	{
	case EGizmoMode::Scale:
		SetRelativeRotation(ActorRot);
		MeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::ScaleGizmo);
		break;

	case EGizmoMode::Rotate:
		SetRelativeRotation(bIsWorldSpace ? FRotator() : ActorRot);
		MeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::RotGizmo);
		break;

	case EGizmoMode::Translate:
		SetRelativeRotation(bIsWorldSpace ? FRotator() : ActorRot);
		MeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::TransGizmo);
		break;
	}
}

float UGizmoComponent::ComputeScreenSpaceScale(const FVector& CameraLocation, bool bIsOrtho, float OrthoWidth)
{
	float NewScale;
	if (bIsOrtho)
	{
		NewScale = OrthoWidth * GizmoScreenScale;
	}
	else
	{
		float Distance = FVector::Distance(CameraLocation, GetWorldLocation());
		NewScale = Distance * GizmoScreenScale;
	}
	return (NewScale < 0.01f) ? 0.01f : NewScale;
}

void UGizmoComponent::ApplyScreenSpaceScaling(const FVector& CameraLocation, bool bIsOrtho, float OrthoWidth)
{
	float NewScale = ComputeScreenSpaceScale(CameraLocation, bIsOrtho, OrthoWidth);
	SetRelativeScale(FVector(NewScale, NewScale, NewScale));
}

void UGizmoComponent::SetWorldSpace(bool bWorldSpace)
{
	bIsWorldSpace = bWorldSpace;
	UpdateGizmoTransform();
}


void UGizmoComponent::UpdateAxisMask(ELevelViewportType ViewportType)
{
	constexpr uint32 AllAxes = 0x7;
	uint32 ViewAxis = AllAxes;

	switch (ViewportType)
	{
	case ELevelViewportType::Top:
	case ELevelViewportType::Bottom:
		ViewAxis = 0x4; break; // Z
	case ELevelViewportType::Front:
	case ELevelViewportType::Back:
		ViewAxis = 0x1; break; // X
	case ELevelViewportType::Left:
	case ELevelViewportType::Right:
		ViewAxis = 0x2; break; // Y
	default: break;
	}

	if (ViewAxis == AllAxes)
	{
		AxisMask = AllAxes;
	}
	else if (CurMode == EGizmoMode::Rotate)
	{
		AxisMask = ViewAxis;            // Rotate: 시선 축만
	}
	else
	{
		AxisMask = AllAxes & ~ViewAxis;  // Translate/Scale: 시선 축 제외
	}
}

void UGizmoComponent::Deactivate()
{
	TargetActor = nullptr;
	AllSelectedActors = nullptr;
	SetVisibility(false);
	SelectedAxis = -1;
}

FMeshBuffer* UGizmoComponent::GetMeshBuffer() const
{
	EMeshShape Shape = EMeshShape::TransGizmo;
	switch (CurMode)
	{
	case EGizmoMode::Translate:
		break;
	case EGizmoMode::Rotate:
		Shape = EMeshShape::RotGizmo;
		break;
	case EGizmoMode::Scale:
		Shape = EMeshShape::ScaleGizmo;
		break;
	}
	return &FMeshBufferManager::Get().GetMeshBuffer(Shape);
}

void UGizmoComponent::CollectRender(FRenderBus& Bus) const
{
	if (!Bus.GetShowFlags().bGizmo) return;
	if (!IsVisible()) return;

	FMeshBuffer* GizmoMesh = GetMeshBuffer();

	// Per-viewport 스케일 계산 — 활성 뷰포트의 스케일과 독립
	const FVector CameraPos = Bus.GetView().GetInverseFast().GetLocation();
	float PerViewScale = const_cast<UGizmoComponent*>(this)->ComputeScreenSpaceScale(
		CameraPos, Bus.IsOrtho(), Bus.GetOrthoWidth());

	// WorldMatrix를 per-viewport 스케일로 재구성 (S * R * T)
	FMatrix WorldMatrix = FMatrix::MakeScaleMatrix(FVector(PerViewScale, PerViewScale, PerViewScale))
		* FMatrix::MakeRotationEuler(GetRelativeRotation().ToVector())
		* FMatrix::MakeTranslationMatrix(GetWorldLocation());

	auto CreateGizmoCmd = [&](bool bInner) {
		FRenderCommand Cmd = {};
		Cmd.Shader = FShaderManager::Get().GetShader(EShaderType::Gizmo);
		Cmd.MeshBuffer = GizmoMesh;
		Cmd.PerObjectConstants = FPerObjectConstants{ WorldMatrix };

		auto& G = Cmd.ExtraCB.Bind<FGizmoConstants>(
			FConstantBufferPool::Get().GetBuffer(ECBSlot::Gizmo, sizeof(FGizmoConstants)), ECBSlot::Gizmo);
		G.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		G.bIsInnerGizmo = bInner ? 1 : 0;
		G.bClicking = bIsHolding ? 1 : 0;
		G.SelectedAxis = SelectedAxis >= 0 ? (uint32)SelectedAxis : 0xffffffffu;
		G.HoveredAxisOpacity = 0.7f;
		G.AxisMask = AxisMask;

		return Cmd;
		};

	Bus.AddCommand(ERenderPass::GizmoOuter, CreateGizmoCmd(false));
	Bus.AddCommand(ERenderPass::GizmoInner, CreateGizmoCmd(true));
}