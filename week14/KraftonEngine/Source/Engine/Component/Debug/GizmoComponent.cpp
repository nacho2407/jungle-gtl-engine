#include "GizmoComponent.h"
#include "Object/Reflection/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Math/Quat.h"
#include "Math/Matrix.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Shader/ShaderManager.h"
#include "Collision/Ray/RayUtils.h"
#include "Render/Proxy/GizmoSceneProxy.h"
#include "Render/Scene/FScene.h"
#include <cfloat>

HIDE_FROM_COMPONENT_LIST(UGizmoComponent)

FPrimitiveSceneProxy* UGizmoComponent::CreateSceneProxy()
{
	return new FGizmoSceneProxy(this, false); // Outer
}

void UGizmoComponent::CreateRenderState()
{
	if (!bSceneRenderingEnabled) return;
	if (SceneProxy) return;

	FScene* Scene = RegisteredScene;
	if (!Scene && Owner && Owner->GetWorld())
		Scene = &Owner->GetWorld()->GetScene();
	if (!Scene) return;

	// Outer 프록시 (기본 경로)
	SceneProxy = Scene->AddPrimitive(this);

	// Inner 프록시 (별도 등록)
	InnerProxy = new FGizmoSceneProxy(this, true);
	Scene->RegisterProxy(InnerProxy);
}

void UGizmoComponent::DestroyRenderState()
{
	FScene* Scene = RegisteredScene;
	if (!Scene && Owner && Owner->GetWorld())
		Scene = &Owner->GetWorld()->GetScene();

	if (Scene)
	{
		if (InnerProxy) { Scene->RemovePrimitive(InnerProxy); InnerProxy = nullptr; }
		if (SceneProxy) { Scene->RemovePrimitive(SceneProxy); SceneProxy = nullptr; }
	}
}

#include <cmath>

namespace
{
	constexpr float AngularDragPlaneParallelTolerance = 1.0e-6f;
	constexpr float AngularDragDirectionMinLengthSquared = 1.0e-8f;
	constexpr float AngularDragMinDeltaRadians = 1.0e-5f;
}

UGizmoComponent::UGizmoComponent()
{
	MeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::TransGizmo);
	LocalExtents = FVector(1.5f, 1.5f, 1.5f);
}

void UGizmoComponent::SetHolding(bool bHold)
{
	if (bIsHolding == bHold)
	{
		return;
	}

	UWorld* World = nullptr;
	if (Target)
	{
		World = Target->GetWorld();
	}
	if (!World && Owner)
	{
		World = Owner->GetWorld();
	}

	if (bHold)
	{
		if (World)
		{
			World->BeginDeferredPickingBVHUpdate();
		}
	}
	else if (World)
	{
		World->EndDeferredPickingBVHUpdate();
	}

	bIsHolding = bHold;
	if (bHold)
	{
		// 새 기즈모 드래그 기준 초기화
		bIsFirstFrameOfDrag = true;
		ResetSnapAccumulation();
		ResetAngularDragBasis();
	}
	else
	{
		// 드래그 종료 후 이전 회전 평면 잔여 상태 제거
		ResetAngularDragBasis();
	}
}

bool UGizmoComponent::IntersectRayAxis(const FRay& Ray, FVector AxisEnd, float AxisScale, float& OutRayT)
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

	//기즈모 픽킹에 원기둥 크기를 반영합니다.
	float ClickThreshold = Radius * AxisScale;
	constexpr float StemRadius = 0.06f;
	ClickThreshold = StemRadius * AxisScale;
	float ClickThresholdSquared = ClickThreshold * ClickThreshold;

	if (DistanceSquared < ClickThresholdSquared)
	{
		OutRayT = RayT;
		return true;
	}

	return false;
}

bool UGizmoComponent::IntersectRayRotationHandle(const FRay& Ray, int32 Axis, float& OutRayT) const
{
	const FVector AxisVector = GetVectorForAxis(Axis).Normalized();
	const float Scale = (Axis == 0) ? GetWorldScale().X : (Axis == 1 ? GetWorldScale().Y : GetWorldScale().Z);
	const float RingRadius = AxisLength * Scale;
	const float RingThickness = Radius * Scale * 1.75f;

	const float Denom = Ray.Direction.Dot(AxisVector);
	if (std::abs(Denom) < 1e-6f)
	{
		return false;
	}

	const float RayT = (GetWorldLocation() - Ray.Origin).Dot(AxisVector) / Denom;
	if (RayT <= 0.0f)
	{
		return false;
	}

	const FVector HitPoint = Ray.Origin + Ray.Direction * RayT;
	const FVector Radial = HitPoint - GetWorldLocation();
	const FVector Planar = Radial - AxisVector * Radial.Dot(AxisVector);
	const float DistanceToRing = std::abs(Planar.Length() - RingRadius);
	if (DistanceToRing <= RingThickness)
	{
		OutRayT = RayT;
		return true;
	}

	return false;
}

void UGizmoComponent::HandleDrag(float DragAmount)
{
	// Snap is applied on the accumulated drag so mouse deltas do not jitter between steps.
	DragAmount = ApplySnapToDragAmount(DragAmount);
	if (DragAmount == 0.0f)
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

float UGizmoComponent::ApplySnapToDragAmount(float DragAmount)
{
	bool bSnapEnabled = false;
	float SnapSize = 0.0f;
	switch (CurMode)
	{
	case EGizmoMode::Translate:
		bSnapEnabled = bTranslationSnapEnabled;
		SnapSize = TranslationSnapSize;
		break;
	case EGizmoMode::Rotate:
		bSnapEnabled = bRotationSnapEnabled;
		SnapSize = RotationSnapSizeRadians;
		break;
	case EGizmoMode::Scale:
		bSnapEnabled = bScaleSnapEnabled;
		SnapSize = ScaleSnapSize;
		break;
	default:
		break;
	}

	if (!bSnapEnabled || SnapSize <= FMath::Epsilon)
	{
		return DragAmount;
	}

	AccumulatedRawDragAmount += DragAmount;
	const float SnappedTotal = std::floor((AccumulatedRawDragAmount / SnapSize) + 0.5f) * SnapSize;
	const float DeltaToApply = SnappedTotal - LastAppliedSnappedDragAmount;
	LastAppliedSnappedDragAmount = SnappedTotal;
	return DeltaToApply;
}

void UGizmoComponent::ResetSnapAccumulation()
{
	AccumulatedRawDragAmount = 0.0f;
	LastAppliedSnappedDragAmount = 0.0f;
}

void UGizmoComponent::ResetAngularDragBasis()
{
	// 회전 드래그 시작 시점에 캡처한 기준 평면 상태 제거
	bAngularDragBasisValid = false;
	AngularDragPivotLocation = FVector::ZeroVector;
	AngularDragAxisVector = FVector::ZeroVector;
	LastAngularDragDirection = FVector::ZeroVector;
}

bool UGizmoComponent::TryGetAngularDragDirection(const FRay& Ray, FVector& OutDirection) const
{
	OutDirection = FVector::ZeroVector;
	if (!bAngularDragBasisValid)
	{
		return false;
	}

	// 드래그 시작 시점의 축을 normal로 쓰는 고정 회전 평면과 ray의 교차점 계산
	const float Denom = Ray.Direction.Dot(AngularDragAxisVector);
	if (std::abs(Denom) < AngularDragPlaneParallelTolerance)
	{
		return false;
	}

	const float DistanceToPlane = (AngularDragPivotLocation - Ray.Origin).Dot(AngularDragAxisVector) / Denom;
	if (DistanceToPlane <= 0.0f)
	{
		return false;
	}

	const FVector CurrentIntersectionLocation = Ray.Origin + (Ray.Direction * DistanceToPlane);
	FVector CurrentDirection = CurrentIntersectionLocation - AngularDragPivotLocation;
	const float DirectionLengthSquared = CurrentDirection.LengthSquared();
	if (DirectionLengthSquared < AngularDragDirectionMinLengthSquared)
	{
		return false;
	}

	// 반지름 크기는 회전량 계산에 불필요하므로 방향만 정규화
	CurrentDirection /= std::sqrt(DirectionLengthSquared);
	OutDirection = CurrentDirection;
	return true;
}

bool UGizmoComponent::HasMultipleSelectedActorTargets() const
{
	if (!AllSelectedActors)
	{
		return false;
	}

	int32 ValidActorCount = 0;
	for (AActor* Actor : *AllSelectedActors)
	{
		if (Actor && Actor->GetRootComponent())
		{
			++ValidActorCount;
			if (ValidActorCount > 1)
			{
				return true;
			}
		}
	}

	return false;
}

FVector UGizmoComponent::GetTargetPivotLocation() const
{
	if (!HasMultipleSelectedActorTargets())
	{
		return Target ? Target->GetWorldLocation() : GetWorldLocation();
	}

	FVector Sum = FVector::ZeroVector;
	int32 ValidActorCount = 0;
	for (AActor* Actor : *AllSelectedActors)
	{
		if (Actor && Actor->GetRootComponent())
		{
			Sum += Actor->GetActorLocation();
			++ValidActorCount;
		}
	}

	return ValidActorCount > 0
		? Sum / static_cast<float>(ValidActorCount)
		: (Target ? Target->GetWorldLocation() : GetWorldLocation());
}

bool UGizmoComponent::TranslateSelectedActorTargets(const FVector& Delta)
{
	if (!HasMultipleSelectedActorTargets())
	{
		return false;
	}

	for (AActor* Actor : *AllSelectedActors)
	{
		if (Actor && Actor->GetRootComponent())
		{
			Actor->AddActorWorldOffset(Delta);
		}
	}

	return true;
}

bool UGizmoComponent::RotateSelectedActorTargets(const FQuat& DeltaQuat)
{
	if (!HasMultipleSelectedActorTargets())
	{
		return false;
	}

	// 다중 선택 회전은 primary target만이 아니라 선택된 모든 actor root에 동일한 회전 delta를 적용합니다.
	for (AActor* Actor : *AllSelectedActors)
	{
		if (Actor && Actor->GetRootComponent())
		{
			USceneComponent* RootComponent = Actor->GetRootComponent();
			FQuat CurrentRotation = RootComponent->GetRelativeQuat();
			FQuat NewRotation = bIsWorldSpace
				? DeltaQuat * CurrentRotation
				: CurrentRotation * DeltaQuat;
			RootComponent->SetRelativeRotation(NewRotation);
		}
	}

	return true;
}

bool UGizmoComponent::ScaleSelectedActorTargets(const FVector& Delta)
{
	if (!HasMultipleSelectedActorTargets())
	{
		return false;
	}

	// 다중 선택 scale도 translate와 동일하게 각 actor의 기존 scale에 delta만 더합니다.
	for (AActor* Actor : *AllSelectedActors)
	{
		if (Actor && Actor->GetRootComponent())
		{
			USceneComponent* RootComponent = Actor->GetRootComponent();
			FVector NewScale = RootComponent->GetRelativeScale() + Delta;
			if (NewScale.X < 0.001f) NewScale.X = 0.001f;
			if (NewScale.Y < 0.001f) NewScale.Y = 0.001f;
			if (NewScale.Z < 0.001f) NewScale.Z = 0.001f;
			RootComponent->SetRelativeScale(NewScale);
		}
	}

	return true;
}

void UGizmoComponent::TranslateTarget(float DragAmount)
{
	if (!Target) return;

	FVector Delta = GetVectorForAxis(SelectedAxis) * DragAmount;
	AddWorldOffset(Delta);
	if (!TranslateSelectedActorTargets(Delta))
	{
		Target->AddWorldOffset(Delta);
	}
}

void UGizmoComponent::RotateTarget(float DragAmount)
{
	if (!Target || !Target->IsValid()) return;

	// World 회전은 드래그 시작 시점의 축을 우선 사용해 회전 중 기즈모 transform 변화에 흔들리지 않게 합니다.
	FVector Axis = bIsWorldSpace
		? (bAngularDragBasisValid ? AngularDragAxisVector : GetVectorForAxis(SelectedAxis))
		: GetLocalAxisVector(SelectedAxis);
	FQuat DeltaQuat = FQuat::FromAxisAngle(Axis, DragAmount);
	if (!RotateSelectedActorTargets(DeltaQuat))
	{
		Target->AddWorldRotation(DeltaQuat, bIsWorldSpace);
	}
}

void UGizmoComponent::ScaleTarget(float DragAmount)
{
	if (!Target || !Target->IsValid()) return;

	FVector Delta = FVector::ZeroVector;
	const float ScaleDelta = DragAmount * ScaleSensitivity;

	if (SelectedAxis == 0) Delta.X = ScaleDelta;
	if (SelectedAxis == 1) Delta.Y = ScaleDelta;
	if (SelectedAxis == 2) Delta.Z = ScaleDelta;

	if (!ScaleSelectedActorTargets(Delta))
	{
		Target->AddScaleDelta(Delta);
	}
}

bool UGizmoComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	OutHitResult = {};
	if (!MeshData || MeshData->Indices.empty())
	{
		return false;
	}

	float BestRayT = FLT_MAX;
	int32 BestAxis = -1;
	const FVector GizmoLocation = GetWorldLocation();

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if ((AxisMask & (1u << Axis)) == 0)
		{
			continue;
		}

		float RayT = 0.0f;
		bool bAxisHit = false;
		if (CurMode == EGizmoMode::Rotate)
		{
			bAxisHit = IntersectRayRotationHandle(Ray, Axis, RayT);
		}
		else
		{
			const FVector AxisDir = GetVectorForAxis(Axis).Normalized();
			const float AxisScale = (Axis == 0) ? GetWorldScale().X : (Axis == 1 ? GetWorldScale().Y : GetWorldScale().Z);
			const FVector AxisEnd = GizmoLocation + AxisDir * AxisLength * AxisScale;
			bAxisHit = IntersectRayAxis(Ray, AxisEnd, AxisScale, RayT);
		}

		if (bAxisHit && RayT < BestRayT)
		{
			BestRayT = RayT;
			BestAxis = Axis;
		}
	}

	if (BestAxis >= 0)
	{
		OutHitResult.bHit = true;
		OutHitResult.Distance = BestRayT;
		OutHitResult.HitComponent = this;
		if (!IsHolding())
		{
			SelectedAxis = BestAxis;
		}
		return true;
	}

	if (!IsHolding())
	{
		SelectedAxis = -1;
	}
	return false;
}


FVector UGizmoComponent::GetVectorForAxis(int32 Axis) const
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

FVector UGizmoComponent::GetLocalAxisVector(int32 Axis) const
{
	switch (Axis)
	{
	case 0:
		return FVector::ForwardVector;
	case 1:
		return FVector::RightVector;
	case 2:
		return FVector::UpVector;
	default:
		return FVector(0.f, 0.f, 0.f);
	}
}

void UGizmoComponent::SetTarget(IGizmoTransformTarget* NewTarget)
{
	Target = NewTarget;

	if (!Target || !Target->IsValid())
	{
		Deactivate();
		return;
	}

	SetWorldLocation(Target->GetWorldLocation());
	UpdateGizmoTransform();
	SetVisibility(true);
}

void UGizmoComponent::SetTarget(USceneComponent* NewTarget)
{
	if (!NewTarget)
	{
		Deactivate();
		return;
	}

	ComponentTarget.SetComponent(NewTarget);
	SetTarget(&ComponentTarget);
}

void UGizmoComponent::SetTarget(AActor* NewTarget)
{
	SetTarget(NewTarget ? NewTarget->GetRootComponent() : nullptr);
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
	if (bIsFirstFrameOfDrag)
	{
		// 회전 시작 순간의 pivot/axis를 고정해 이후 frame의 target 회전 변화가 계산 기준을 흔들지 못하게 합니다.
		AngularDragPivotLocation = GetWorldLocation();
		AngularDragAxisVector = GetVectorForAxis(SelectedAxis).Normalized();
		if (AngularDragAxisVector.LengthSquared() < AngularDragDirectionMinLengthSquared)
		{
			ResetAngularDragBasis();
			return;
		}

		bAngularDragBasisValid = true;
		if (!TryGetAngularDragDirection(Ray, LastAngularDragDirection))
		{
			ResetAngularDragBasis();
			return;
		}

		bIsFirstFrameOfDrag = false;
		return;
	}

	FVector CurrentAngularDragDirection;
	if (!TryGetAngularDragDirection(Ray, CurrentAngularDragDirection))
	{
		return;
	}

	// atan2 기반 signed angle 계산으로 아주 작은 회전과 부호 판정을 안정화합니다.
	const float CosAngle = Clamp(LastAngularDragDirection.Dot(CurrentAngularDragDirection), -1.0f, 1.0f);
	const float SignedSinAngle = LastAngularDragDirection.Cross(CurrentAngularDragDirection).Dot(AngularDragAxisVector);
	const float DeltaAngle = std::atan2(SignedSinAngle, CosAngle);
	LastAngularDragDirection = CurrentAngularDragDirection;

	if (std::abs(DeltaAngle) < AngularDragMinDeltaRadians)
	{
		return;
	}

	HandleDrag(DeltaAngle);
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
	if (!IsHolding() || !IsActive()) return;
	if (SelectedAxis == -1 || !Target || !Target->IsValid()) return;

	CurMode == EGizmoMode::Rotate ? UpdateAngularDrag(Ray) : UpdateLinearDrag(Ray);
}

void UGizmoComponent::DragEnd()
{
	bIsFirstFrameOfDrag = true;
	// 다음 드래그가 이전 snap/회전 평면 상태를 이어받지 않도록 초기화
	ResetSnapAccumulation();
	ResetAngularDragBasis();
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
	if (CurMode != NewMode)
	{
		// 드래그 중 모드가 바뀌는 예외 상황에서도 이전 회전 기준 제거
		bIsFirstFrameOfDrag = true;
		ResetSnapAccumulation();
		ResetAngularDragBasis();
	}

	CurMode = NewMode;
	UpdateGizmoTransform();
}

void UGizmoComponent::UpdateGizmoTransform()
{
	if (!Target) return;

	const FVector PivotLocation = GetTargetPivotLocation();
	SetWorldLocation(PivotLocation);

	if (bIsWorldSpace && CurMode != EGizmoMode::Scale)
	{
		SetRelativeRotation(FRotator::ZeroRotator);
	}
	else
	{
		SetRelativeRotation(Target->GetWorldRotation());
	}

	const FMeshData* DesiredMeshData = nullptr;

	switch (CurMode)
	{
	case EGizmoMode::Scale:
		DesiredMeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::ScaleGizmo);
		break;

	case EGizmoMode::Rotate:
		DesiredMeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::RotGizmo);
		break;

	case EGizmoMode::Translate:
		DesiredMeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::TransGizmo);
		break;

	default:
		break;
	}

	if (FVector::DistSquared(GetWorldLocation(), PivotLocation) > FMath::Epsilon * FMath::Epsilon)
	{
		SetWorldLocation(PivotLocation);
	}

	if (MeshData != DesiredMeshData && DesiredMeshData)
	{
		MeshData = DesiredMeshData;
		MarkRenderStateDirty();
	}
}

float UGizmoComponent::ComputeScreenSpaceScale(const FVector& CameraLocation, bool bIsOrtho, float OrthoWidth) const
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

void UGizmoComponent::SetSceneRenderingEnabled(bool bEnabled)
{
	if (bSceneRenderingEnabled == bEnabled)
	{
		return;
	}

	bSceneRenderingEnabled = bEnabled;
	if (bSceneRenderingEnabled)
	{
		CreateRenderState();
	}
	else
	{
		DestroyRenderState();
	}
}

void UGizmoComponent::SetSnapSettings(bool bTranslationEnabled, float InTranslationSnapSize,
	bool bRotationEnabled, float InRotationSnapSizeDegrees,
	bool bScaleEnabled, float InScaleSnapSize)
{
	bTranslationSnapEnabled = bTranslationEnabled;
	TranslationSnapSize = (InTranslationSnapSize > FMath::Epsilon) ? InTranslationSnapSize : 10.0f;
	bRotationSnapEnabled = bRotationEnabled;
	RotationSnapSizeRadians = ((InRotationSnapSizeDegrees > FMath::Epsilon) ? InRotationSnapSizeDegrees : 15.0f) * DEG_TO_RAD;
	bScaleSnapEnabled = bScaleEnabled;
	ScaleSnapSize = (InScaleSnapSize > FMath::Epsilon) ? InScaleSnapSize : 0.1f;
}

uint32 UGizmoComponent::ComputeAxisMask(ELevelViewportType ViewportType, EGizmoMode Mode)
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
		return AllAxes;

	if (Mode == EGizmoMode::Rotate)
		return ViewAxis;            // Rotate: 시선 축만

	return AllAxes & ~ViewAxis;     // Translate/Scale: 시선 축 제외
}

void UGizmoComponent::Deactivate()
{
	if (bIsHolding)
	{
		SetHolding(false);
	}

	bIsFirstFrameOfDrag = true;
	ResetSnapAccumulation();
	ResetAngularDragBasis();
	Target = nullptr;
	ComponentTarget.SetComponent(nullptr);
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
