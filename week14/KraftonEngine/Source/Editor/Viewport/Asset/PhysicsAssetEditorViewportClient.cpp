#include "Editor/Viewport/Asset/PhysicsAssetEditorViewportClient.h"

#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Input/InputSystem.h"
#include "Math/MathUtils.h"
#include "Physics/BodySetup.h"
#include "Physics/PhysicsAsset.h"
#include "Physics/PhysicsAssetDebugDraw.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"
#include "Viewport/Viewport.h"

#include <imgui.h>
#include <cfloat>
#include <cmath>

namespace
{
	bool IntersectRaySphere(const FVector& Origin, const FVector& Direction, const FVector& Center, float Radius, float& OutT)
	{
		const FVector M = Origin - Center;
		const float B = M.Dot(Direction);
		const float C = M.Dot(M) - Radius * Radius;
		if (C > 0.0f && B > 0.0f)
		{
			return false;
		}

		const float Discriminant = B * B - C;
		if (Discriminant < 0.0f)
		{
			return false;
		}

		OutT = -B - std::sqrt(Discriminant);
		if (OutT < 0.0f)
		{
			OutT = 0.0f;
		}
		return true;
	}

	bool IntersectRayAABB(const FVector& Origin, const FVector& Direction, const FVector& Extents, float& OutT)
	{
		float TMin = 0.0f;
		float TMax = FLT_MAX;

		const float O[3] = { Origin.X, Origin.Y, Origin.Z };
		const float D[3] = { Direction.X, Direction.Y, Direction.Z };
		const float E[3] = { Extents.X, Extents.Y, Extents.Z };

		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			if (std::fabs(D[Axis]) < 1e-6f)
			{
				if (O[Axis] < -E[Axis] || O[Axis] > E[Axis])
				{
					return false;
				}
				continue;
			}

			float T1 = (-E[Axis] - O[Axis]) / D[Axis];
			float T2 = ( E[Axis] - O[Axis]) / D[Axis];
			if (T1 > T2)
			{
				const float Temp = T1;
				T1 = T2;
				T2 = Temp;
			}

			if (T1 > TMin) TMin = T1;
			if (T2 < TMax) TMax = T2;
			if (TMin > TMax)
			{
				return false;
			}
		}

		OutT = TMin;
		return true;
	}

	bool IntersectRayCapsuleZ(const FVector& Origin, const FVector& Direction, float Radius, float Length, float& OutT)
	{
		bool bHit = false;
		float BestT = FLT_MAX;
		const float HalfLength = Length * 0.5f;

		const float A = Direction.X * Direction.X + Direction.Y * Direction.Y;
		const float B = 2.0f * (Origin.X * Direction.X + Origin.Y * Direction.Y);
		const float C = Origin.X * Origin.X + Origin.Y * Origin.Y - Radius * Radius;
		const float Discriminant = B * B - 4.0f * A * C;
		if (A > 1e-6f && Discriminant >= 0.0f)
		{
			const float SqrtD = std::sqrt(Discriminant);
			const float Denom = 2.0f * A;
			const float Candidates[2] = { (-B - SqrtD) / Denom, (-B + SqrtD) / Denom };
			for (float T : Candidates)
			{
				if (T < 0.0f || T >= BestT)
				{
					continue;
				}

				const float Z = Origin.Z + Direction.Z * T;
				if (Z >= -HalfLength && Z <= HalfLength)
				{
					BestT = T;
					bHit = true;
				}
			}
		}

		float CapT = 0.0f;
		if (IntersectRaySphere(Origin, Direction, FVector(0.0f, 0.0f, HalfLength), Radius, CapT) && CapT < BestT)
		{
			BestT = CapT;
			bHit = true;
		}
		if (IntersectRaySphere(Origin, Direction, FVector(0.0f, 0.0f, -HalfLength), Radius, CapT) && CapT < BestT)
		{
			BestT = CapT;
			bHit = true;
		}

		OutT = BestT;
		return bHit;
	}

	void ConsiderHit(
		const FRay& WorldRay,
		const FMatrix& BoneMatrix,
		const FVector& BoneLocalHit,
		int32 BodyIndex,
		EPhysicsAssetShapeType ShapeType,
		int32 ShapeIndex,
		FPhysicsAssetEditorHitResult& InOutBest)
	{
		const FVector WorldHit = BoneMatrix.TransformPositionWithW(BoneLocalHit);
		const float Distance = (WorldHit - WorldRay.Origin).Dot(WorldRay.Direction);
		if (Distance < 0.0f)
		{
			return;
		}

		if (!InOutBest.bHit || Distance < InOutBest.Distance)
		{
			InOutBest.bHit = true;
			InOutBest.BodyIndex = BodyIndex;
			InOutBest.ShapeType = ShapeType;
			InOutBest.ShapeIndex = ShapeIndex;
			InOutBest.Distance = Distance;
			InOutBest.WorldPosition = WorldHit;
		}
	}
}

void FPhysicsAssetEditorViewportClient::Initialize(ID3D11Device* Device, uint32 Width, uint32 Height)
{
	Viewport = new FViewport();
	Viewport->Initialize(Device, Width, Height);
	Viewport->SetClient(this);

	RenderOptions.ShowFlags.bDebugDraw = true;
	RenderOptions.ShowFlags.bShowCollisionShape = true;
	bIsRenderable = true;
}

void FPhysicsAssetEditorViewportClient::Release()
{
	if (Viewport)
	{
		Viewport->Release();
		delete Viewport;
		Viewport = nullptr;
	}

	PreviewWorld = nullptr;
	PhysicsAsset = nullptr;
	PreviewMeshComponent = nullptr;
	bIsRenderable = false;
}

void FPhysicsAssetEditorViewportClient::SetPreviewScene(UWorld* InWorld, UPhysicsAsset* InAsset, USkeletalMeshComponent* InMeshComponent)
{
	PreviewWorld = InWorld;
	PhysicsAsset = InAsset;
	PreviewMeshComponent = InMeshComponent;
	SetShowPreviewMesh(bShowPreviewMesh);
}

void FPhysicsAssetEditorViewportClient::ResetCameraToPreviewBounds()
{
	if (!PreviewMeshComponent)
	{
		ViewTransform.ViewLocation = FVector(-5.0f, -5.0f, 3.0f);
		ViewTransform.LookAt(FVector::ZeroVector);
		TargetLocation = ViewTransform.ViewLocation;
		LastAppliedCameraLocation = ViewTransform.ViewLocation;
		bTargetLocationInitialized = true;
		bLastAppliedCameraLocationInitialized = true;
		return;
	}

	const FBoundingBox Bounds = PreviewMeshComponent->GetWorldBoundingBox();
	const FVector Center = Bounds.GetCenter();
	float Radius = Bounds.GetExtent().Length();
	if (Radius < 0.1f)
	{
		Radius = 1.0f;
	}

	const float Distance = Radius / std::tan(ViewTransform.FOV * 0.5f) * 1.35f;
	const FVector ViewDir = FVector(-1.0f, -1.0f, -0.55f).Normalized();

	ViewTransform.ViewLocation = Center - ViewDir * Distance;
	ViewTransform.LookAt(Center);

	TargetLocation = ViewTransform.ViewLocation;
	LastAppliedCameraLocation = ViewTransform.ViewLocation;
	bTargetLocationInitialized = true;
	bLastAppliedCameraLocationInitialized = true;
}

void FPhysicsAssetEditorViewportClient::SetShowPreviewMesh(bool bInShow)
{
	bShowPreviewMesh = bInShow;
	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->SetVisibility(bShowPreviewMesh);
	}
}

bool FPhysicsAssetEditorViewportClient::GetMouseRay(FRay& OutRay) const
{
	if (!Viewport || ViewportScreenRect.Width <= 1.0f || ViewportScreenRect.Height <= 1.0f)
	{
		return false;
	}

	const ImVec2 MousePos = ImGui::GetIO().MousePos;
	const float LocalMouseX = MousePos.x - ViewportScreenRect.X;
	const float LocalMouseY = MousePos.y - ViewportScreenRect.Y;
	if (LocalMouseX < 0.0f || LocalMouseY < 0.0f || LocalMouseX >= ViewportScreenRect.Width || LocalMouseY >= ViewportScreenRect.Height)
	{
		return false;
	}

	FMinimalViewInfo POV;
	GetCameraView(POV);
	OutRay = POV.DeprojectScreenToWorld(
		LocalMouseX,
		LocalMouseY,
		static_cast<float>(Viewport->GetWidth()),
		static_cast<float>(Viewport->GetHeight()));
	return true;
}

bool FPhysicsAssetEditorViewportClient::PickBodyShapeAtMouse(FPhysicsAssetEditorHitResult& OutHit) const
{
	OutHit = FPhysicsAssetEditorHitResult {};
	if (!PhysicsAsset || !PreviewMeshComponent)
	{
		return false;
	}

	FRay WorldRay;
	if (!GetMouseRay(WorldRay))
	{
		return false;
	}

	const TArray<UBodySetup*>& Bodies = PhysicsAsset->GetBodySetups();
	for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(Bodies.size()); ++BodyIndex)
	{
		const UBodySetup* Body = Bodies[BodyIndex];
		if (!Body)
		{
			continue;
		}

		FMatrix BoneMatrix;
		if (!PreviewMeshComponent->GetBoneWorldMatrixByName(Body->GetBoneName().ToString(), BoneMatrix))
		{
			continue;
		}

		const FMatrix InvBone = BoneMatrix.GetInverse();
		const FVector LocalOrigin = InvBone.TransformPositionWithW(WorldRay.Origin);
		const FVector LocalEnd = InvBone.TransformPositionWithW(WorldRay.Origin + WorldRay.Direction);
		const FVector LocalDir = (LocalEnd - LocalOrigin).Normalized();
		if (LocalDir.IsNearlyZero())
		{
			continue;
		}

		const FKAggregateGeom& Geom = Body->GetAggGeom();

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(Geom.SphereElems.size()); ++ShapeIndex)
		{
			const FKSphereElem& Sphere = Geom.SphereElems[ShapeIndex];
			float T = 0.0f;
			if (IntersectRaySphere(LocalOrigin, LocalDir, Sphere.Center, Sphere.Radius, T))
			{
				ConsiderHit(WorldRay, BoneMatrix, LocalOrigin + LocalDir * T, BodyIndex, EPhysicsAssetShapeType::Sphere, ShapeIndex, OutHit);
			}
		}

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(Geom.BoxElems.size()); ++ShapeIndex)
		{
			const FKBoxElem& Box = Geom.BoxElems[ShapeIndex];
			const FQuat InvRot = Box.Rotation.Inverse();
			const FVector ShapeOrigin = InvRot.RotateVector(LocalOrigin - Box.Center);
			const FVector ShapeDir = InvRot.RotateVector(LocalDir).Normalized();
			float T = 0.0f;
			if (IntersectRayAABB(ShapeOrigin, ShapeDir, Box.Extents, T))
			{
				const FVector ShapeHit = ShapeOrigin + ShapeDir * T;
				const FVector BoneLocalHit = Box.Center + Box.Rotation.RotateVector(ShapeHit);
				ConsiderHit(WorldRay, BoneMatrix, BoneLocalHit, BodyIndex, EPhysicsAssetShapeType::Box, ShapeIndex, OutHit);
			}
		}

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(Geom.SphylElems.size()); ++ShapeIndex)
		{
			const FKSphylElem& Capsule = Geom.SphylElems[ShapeIndex];
			const FQuat InvRot = Capsule.Rotation.Inverse();
			const FVector ShapeOrigin = InvRot.RotateVector(LocalOrigin - Capsule.Center);
			const FVector ShapeDir = InvRot.RotateVector(LocalDir).Normalized();
			float T = 0.0f;
			if (IntersectRayCapsuleZ(ShapeOrigin, ShapeDir, Capsule.Radius, Capsule.Length, T))
			{
				const FVector ShapeHit = ShapeOrigin + ShapeDir * T;
				const FVector BoneLocalHit = Capsule.Center + Capsule.Rotation.RotateVector(ShapeHit);
				ConsiderHit(WorldRay, BoneMatrix, BoneLocalHit, BodyIndex, EPhysicsAssetShapeType::Sphyl, ShapeIndex, OutHit);
			}
		}
	}

	return OutHit.bHit;
}

void FPhysicsAssetEditorViewportClient::ClearHighlight()
{
	HighlightBodyIndex = -1;
	HighlightShapeIndex = -1;
	HighlightConstraintIndex = -1;
}

void FPhysicsAssetEditorViewportClient::SetHighlightedBody(int32 BodyIndex)
{
	ClearHighlight();
	HighlightBodyIndex = BodyIndex;
}

void FPhysicsAssetEditorViewportClient::SetHighlightedShape(int32 BodyIndex, EPhysicsAssetShapeType ShapeType, int32 ShapeIndex)
{
	ClearHighlight();
	HighlightBodyIndex = BodyIndex;
	HighlightShapeType = ShapeType;
	HighlightShapeIndex = ShapeIndex;
}

void FPhysicsAssetEditorViewportClient::SetHighlightedConstraint(int32 ConstraintIndex)
{
	ClearHighlight();
	HighlightConstraintIndex = ConstraintIndex;
}

bool FPhysicsAssetEditorViewportClient::IsMouseOverViewport() const
{
	if (!bIsRenderable || ViewportScreenRect.Width <= 0.0f || ViewportScreenRect.Height <= 0.0f)
	{
		return false;
	}

	const ImVec2 MousePos = ImGui::GetMousePos();
	return MousePos.x >= ViewportScreenRect.X && MousePos.x <= (ViewportScreenRect.X + ViewportScreenRect.Width)
		&& MousePos.y >= ViewportScreenRect.Y && MousePos.y <= (ViewportScreenRect.Y + ViewportScreenRect.Height);
}

void FPhysicsAssetEditorViewportClient::NotifyViewportResized(int32 NewWidth, int32 NewHeight)
{
	if (NewHeight > 0)
	{
		ViewTransform.AspectRatio = static_cast<float>(NewWidth) / static_cast<float>(NewHeight);
	}
}

bool FPhysicsAssetEditorViewportClient::GetCameraView(FMinimalViewInfo& OutPOV) const
{
	OutPOV.Location = ViewTransform.ViewLocation;
	OutPOV.Rotation = ViewTransform.ViewRotation;
	OutPOV.FOV = ViewTransform.FOV;
	OutPOV.AspectRatio = ViewTransform.AspectRatio;
	return true;
}

void FPhysicsAssetEditorViewportClient::SubmitFrameDebugDraw()
{
	DrawPreviewPhysicsAsset();
}

void FPhysicsAssetEditorViewportClient::Tick(float DeltaTime)
{
	SyncCameraSmoothingTarget();
	ApplySmoothedCameraLocation(DeltaTime);
	TickShortcuts();
	TickInput(DeltaTime);
}

void FPhysicsAssetEditorViewportClient::TickShortcuts()
{
	if (!FSlateApplication::Get().DoesClientOwnKeyboardInput(this))
	{
		return;
	}

	if (InputSystem::Get().GetKeyDown('F'))
	{
		ResetCameraToPreviewBounds();
	}
}

void FPhysicsAssetEditorViewportClient::TickInput(float DeltaTime)
{
	if (!FSlateApplication::Get().DoesClientOwnMouseInput(this))
	{
		return;
	}
	if (ImGui::GetIO().WantTextInput)
	{
		return;
	}

	FViewportCameraControlSettings& ControlSettings = FEditorSettings::Get().MeshEditorViewportSettings.CameraControls;
	InputSystem& Input = InputSystem::Get();

	FVector LocalMove = FVector::ZeroVector;
	float WorldVerticalMove = 0.0f;
	const float CameraSpeed = ControlSettings.MoveSpeed;

	if (Input.GetKey('W')) LocalMove.X += CameraSpeed;
	if (Input.GetKey('S')) LocalMove.X -= CameraSpeed;
	if (Input.GetKey('D')) LocalMove.Y += CameraSpeed;
	if (Input.GetKey('A')) LocalMove.Y -= CameraSpeed;
	if (Input.GetKey('Q')) WorldVerticalMove -= CameraSpeed;
	if (Input.GetKey('E')) WorldVerticalMove += CameraSpeed;

	const FVector Forward = ViewTransform.ViewRotation.GetForwardVector();
	const FVector Right = ViewTransform.ViewRotation.GetRightVector();

	FVector DeltaMove = (Forward * LocalMove.X + Right * LocalMove.Y) * DeltaTime;
	DeltaMove.Z += WorldVerticalMove * DeltaTime;
	TargetLocation += DeltaMove;

	if (Input.GetKey(VK_RBUTTON))
	{
		const float MouseRotationSpeed = 0.15f * ControlSettings.RotationSpeed;
		const float DeltaYaw = static_cast<float>(Input.MouseDeltaX()) * MouseRotationSpeed;
		const float DeltaPitch = static_cast<float>(Input.MouseDeltaY()) * MouseRotationSpeed;
		ViewTransform.Rotate(DeltaYaw, DeltaPitch);
	}

	const float ScrollNotches = InputSystem::Get().GetScrollNotches();
	if (ScrollNotches != 0.0f)
	{
		if (InputSystem::Get().GetKey(VK_RBUTTON))
		{
			float& MoveSpeed = FEditorSettings::Get().MeshEditorViewportSettings.CameraControls.MoveSpeed;
			MoveSpeed = ScrollNotches < 0.0f ? MoveSpeed * 0.9f : MoveSpeed * 1.1f;
			MoveSpeed = Clamp(MoveSpeed, 0.001f, 1000.0f);
		}
		else if (ViewTransform.bIsOrtho)
		{
			const float NewWidth = ViewTransform.OrthoZoom - ScrollNotches * ControlSettings.ZoomSpeed * DeltaTime;
			ViewTransform.OrthoZoom = Clamp(NewWidth, 0.1f, 1000.0f);
		}
		else
		{
			TargetLocation += ViewTransform.ViewRotation.GetForwardVector() * (ScrollNotches * ControlSettings.ZoomSpeed * 0.015f);
		}
	}
}

void FPhysicsAssetEditorViewportClient::SyncCameraSmoothingTarget()
{
	const FVector CurrentLocation = ViewTransform.ViewLocation;
	const bool bCameraMovedExternally = bLastAppliedCameraLocationInitialized
		&& FVector::DistSquared(CurrentLocation, LastAppliedCameraLocation) > 0.0001f;

	if (!bTargetLocationInitialized || bCameraMovedExternally)
	{
		TargetLocation = CurrentLocation;
		bTargetLocationInitialized = true;
	}

	LastAppliedCameraLocation = CurrentLocation;
	bLastAppliedCameraLocationInitialized = true;
}

void FPhysicsAssetEditorViewportClient::ApplySmoothedCameraLocation(float DeltaTime)
{
	const FVector CurrentLocation = ViewTransform.ViewLocation;
	const float LerpAlpha = Clamp(DeltaTime * SmoothLocationSpeed, 0.0f, 1.0f);
	const FVector NewLocation = CurrentLocation + (TargetLocation - CurrentLocation) * LerpAlpha;
	ViewTransform.ViewLocation = NewLocation;

	LastAppliedCameraLocation = NewLocation;
	bLastAppliedCameraLocationInitialized = true;
}

void FPhysicsAssetEditorViewportClient::DrawPreviewPhysicsAsset()
{
	if (!PreviewWorld || !PhysicsAsset || !PreviewMeshComponent || (!bShowBodies && !bShowConstraints))
	{
		return;
	}

	FPhysicsAssetDebugDrawOptions Options;
	Options.bDrawBodies = bShowBodies;
	Options.bDrawConstraints = bShowConstraints;
	Options.HighlightBodyIndex = HighlightBodyIndex;
	Options.HighlightShapeType = HighlightShapeType;
	Options.HighlightShapeIndex = HighlightShapeIndex;
	Options.HighlightConstraintIndex = HighlightConstraintIndex;
	DrawPhysicsAssetDebug(PreviewWorld, PhysicsAsset, PreviewMeshComponent, Options);
}
