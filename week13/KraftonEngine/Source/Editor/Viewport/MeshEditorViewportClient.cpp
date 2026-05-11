#include "MeshEditorViewportClient.h"

#include "Render/Types/MinimalViewInfo.h"
#include "Viewport/Viewport.h"
#include "Math/MathUtils.h"
#include "Input/InputSystem.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Component/GizmoComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Collision/RayUtils.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"

#include <imgui.h>

void FMeshEditorViewportClient::Initialize(ID3D11Device* Device, uint32 Width, uint32 Height)
{
	Viewport = new FViewport();
	Viewport->Initialize(Device, Width, Height);
	Viewport->SetClient(this);

	bIsRenderable = true;
}

void FMeshEditorViewportClient::Release()
{
	if (Viewport)
	{
		Viewport->Release();
		delete Viewport;
		Viewport = nullptr;
	}

	PreviewWorld = nullptr;
	PreviewActor = nullptr;

	UObjectManager::Get().DestroyObject(Gizmo);
	Gizmo = nullptr;

	bIsRenderable = false;

	SetSelectedBone(nullptr, -1);
}

void FMeshEditorViewportClient::CreatePreviewGizmo()
{
	Gizmo = UObjectManager::Get().CreateObject<UGizmoComponent>();
	Gizmo->SetScene(&PreviewWorld->GetScene());
	Gizmo->CreateRenderState();
	Gizmo->Deactivate();
}

void FMeshEditorViewportClient::ResetCameraToPreviousBounds()
{
	if (!PreviewActor)
	{
		ViewTransform.ViewLocation = FVector(-5.0f, -5.0f, 3.0f);
		ViewTransform.LookAt(FVector::ZeroVector);
		TargetLocation = ViewTransform.ViewLocation;
		return;
	}

	FBoundingBox Bounds = PreviewMeshComponent->GetWorldBoundingBox();
	FVector Center = Bounds.GetCenter();
	float Radius = Bounds.GetExtent().Length();

	if (Radius < 0.1f)
	{
		Radius = 1.0f;
	}

	const float FovRadians = ViewTransform.FOV;
	const float Distance = Radius / std::tan(FovRadians * 0.5f) * 1.25f;

	const FVector ViewDir = FVector(-1.0f, -1.0f, 0.6f).Normalized();
	
	ViewTransform.ViewLocation = Center - ViewDir * Distance;
	ViewTransform.LookAt(Center);

	TargetLocation = ViewTransform.ViewLocation;
	LastAppliedCameraLocation = ViewTransform.ViewLocation;
	bTargetLocationInitialized = true;
	bLastAppliedCameraLocationInitialized = true;
}

bool FMeshEditorViewportClient::IsMouseOverViewport() const
{
	if (!bIsRenderable || ViewportScreenRect.Width <= 0.0f || ViewportScreenRect.Height <= 0.0f) return false;

	ImVec2 MousePos = ImGui::GetMousePos();
	return MousePos.x >= ViewportScreenRect.X && MousePos.x <= (ViewportScreenRect.X + ViewportScreenRect.Width) &&
		MousePos.y >= ViewportScreenRect.Y && MousePos.y <= (ViewportScreenRect.Y + ViewportScreenRect.Height);
}

bool FMeshEditorViewportClient::IsGizmoHolding() const
{
	return Gizmo && Gizmo->IsHolding();
}

void FMeshEditorViewportClient::NotifyViewportResized(int32 NewWidth, int32 NewHeight)
{
	if (Viewport)
	{
		ViewTransform.AspectRatio = static_cast<float>(NewWidth) / static_cast<float>(NewHeight);
	}
}

bool FMeshEditorViewportClient::GetCameraView(FMinimalViewInfo& OutPOV) const
{
	OutPOV.Location = ViewTransform.ViewLocation;
	OutPOV.Rotation = ViewTransform.ViewRotation;
	OutPOV.FOV = ViewTransform.FOV;
	OutPOV.AspectRatio = ViewTransform.AspectRatio;
	return true;
}

void FMeshEditorViewportClient::Tick(float DeltaTime)
{
	SyncCameraSmoothingTarget();

	if (bIsFocusAnimating)
	{
		FocusAnimTimer += DeltaTime;
		float Alpha = Clamp(FocusAnimTimer / FocusAnimDuration, 0.0f, 1.0f);
		if (Alpha >= 1.0f)
		{
			Alpha = 1.0f;
			bIsFocusAnimating = false;
		}

		float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);

		FVector NewLoc = FocusStartLoc * (1.0f - SmoothAlpha) + FocusEndLoc * SmoothAlpha;

		FQuat StartQuat = FocusStartRot.ToQuaternion();
		FQuat EndQuat = FocusEndRot.ToQuaternion();
		FQuat BlendedQuat = FQuat::Slerp(StartQuat, EndQuat, SmoothAlpha);

		ViewTransform.ViewLocation = NewLoc;
		ViewTransform.ViewRotation = FRotator::FromQuaternion(BlendedQuat);

		TargetLocation = NewLoc;
		LastAppliedCameraLocation = NewLoc;
		bLastAppliedCameraLocationInitialized = true;
	}
	else
	{
		ApplySmoothedCameraLocation(DeltaTime);
	}

	TickShortcuts();
	TickInput(DeltaTime);
	TickInteraction(DeltaTime);
}

void FMeshEditorViewportClient::SetSelectedBone(USkeletalMesh* Mesh, int32 BoneIndex)
{
	SelectedMesh = Mesh;
	SelectedBoneIndex = BoneIndex;

	if (Gizmo && PreviewMeshComponent && BoneIndex >= 0)
	{
		BoneTarget.SetBone(PreviewMeshComponent, BoneIndex);
		Gizmo->SetTarget(&BoneTarget);
	}
	else if (Gizmo)
	{
		Gizmo->Deactivate();
	}
}

const FBone* FMeshEditorViewportClient::GetSelectedBone() const
{
	if (!SelectedMesh) return nullptr;

	FSkeletalMesh* Asset = SelectedMesh->GetSkeletalMeshAsset();
	if (!Asset) return nullptr;

	if (SelectedBoneIndex < 0 || SelectedBoneIndex >= static_cast<int32>(Asset->Bones.size())) return nullptr;

	return &Asset->Bones[SelectedBoneIndex];
}

void FMeshEditorViewportClient::TickShortcuts()
{
	if (!FSlateApplication::Get().DoesClientOwnKeyboardInput(this)) return;

	if (InputSystem::Get().GetKeyDown('F'))
	{
		if (const FBone* SelectedBone = GetSelectedBone())
		{
			FVector TargetLoc = PreviewMeshComponent->GetBoneLocationByIndex(SelectedBoneIndex);

			FVector OriginalLoc = ViewTransform.ViewLocation;
			FRotator OriginalRot = ViewTransform.ViewRotation;

			FBoundingBox Bounds = PreviewMeshComponent->GetWorldBoundingBox();
			FVector Center = Bounds.GetCenter();
			float Radius = Bounds.GetExtent().Length();

			if (Radius < 0.1f)
			{
				Radius = 1.0f;
			}

			float FocusDistance = Radius;
			FVector CameraForward = ViewTransform.ViewRotation.GetForwardVector();
			FVector NewCameraLoc = TargetLoc - CameraForward * FocusDistance;

			ViewTransform.ViewLocation = NewCameraLoc;
			ViewTransform.LookAt(TargetLoc);
			FRotator TargetRot = ViewTransform.ViewRotation;

			ViewTransform.ViewLocation = OriginalLoc;
			ViewTransform.ViewRotation = OriginalRot;

			bIsFocusAnimating = true;
			FocusAnimTimer = 0.0f;
			FocusStartLoc = OriginalLoc;
			FocusStartRot = OriginalRot;
			FocusEndLoc = NewCameraLoc;
			FocusEndRot = TargetRot;
		}
	}
}

void FMeshEditorViewportClient::TickInput(float DeltaTime)
{
	if (!FSlateApplication::Get().DoesClientOwnMouseInput(this)) return;

	if (InputSystem::Get().GetGuiInputState().bUsingKeyboard) return;

	FViewportCameraControlSettings& ControlSettings = FEditorSettings::Get().MeshEditorViewportSettings.CameraControls;

	InputSystem& Input = InputSystem::Get();
	
	FVector LocalMove = FVector::ZeroVector;
	float WorldVerticalMove = 0.0f;
	float CameraSpeed = ControlSettings.MoveSpeed;

	if (Input.GetKey('W')) LocalMove.X += CameraSpeed;
	if (Input.GetKey('S')) LocalMove.X -= CameraSpeed;
	if (Input.GetKey('D')) LocalMove.Y += CameraSpeed;
	if (Input.GetKey('A')) LocalMove.Y -= CameraSpeed;
	if (Input.GetKey('Q')) WorldVerticalMove -= CameraSpeed;
	if (Input.GetKey('E')) WorldVerticalMove += CameraSpeed;

	const FVector Forward = ViewTransform.ViewRotation.GetForwardVector();
	const FVector Right = ViewTransform.ViewRotation.GetRightVector();
	const FVector Up = ViewTransform.ViewRotation.GetUpVector();

	FVector DeltaMove = (Forward * LocalMove.X + Right * LocalMove.Y) * DeltaTime;
	DeltaMove.Z += WorldVerticalMove * DeltaTime;
	TargetLocation += DeltaMove;

	FVector Rotation = FVector::ZeroVector;

	FVector MouseRotation = FVector::ZeroVector;
	float MouseRotationSpeed = 0.15f * ControlSettings.RotationSpeed;

	if (Input.GetKey(VK_RBUTTON))
	{
		float DeltaX = static_cast<float>(Input.MouseDeltaX());
		float DeltaY = static_cast<float>(Input.MouseDeltaY());

		MouseRotation.Y += DeltaX * MouseRotationSpeed;
		MouseRotation.Z += DeltaY * MouseRotationSpeed;
	}

	Rotation *= DeltaTime;
	ViewTransform.Rotate(Rotation.Y + MouseRotation.Y, Rotation.Z + MouseRotation.Z);

	if (Input.GetKeyUp(VK_SPACE))
	{
		Gizmo->SetNextMode();
	}
}

void FMeshEditorViewportClient::TickInteraction(float DeltaTime)
{
	if (!FSlateApplication::Get().DoesClientOwnKeyboardInput(this)) return;

	if (!Gizmo || !PreviewWorld) return;

	FViewportCameraControlSettings& ControlSettings = FEditorSettings::Get().MeshEditorViewportSettings.CameraControls;

	Gizmo->ApplyScreenSpaceScaling(ViewTransform.ViewLocation, ViewTransform.bIsOrtho, ViewTransform.OrthoZoom);
	Gizmo->SetAxisMask(UGizmoComponent::ComputeAxisMask(RenderOptions.ViewportType, Gizmo->GetMode()));

	if (InputSystem::Get().GetGuiInputState().bUsingMouse && !Gizmo->IsHolding()) return;

	const float ZoomSpeed = ControlSettings.ZoomSpeed;

	float ScrollNotches = InputSystem::Get().GetScrollNotches();
	if (ScrollNotches != 0.0f)
	{
		if (ViewTransform.bIsOrtho)
		{
			float NewWidth = ViewTransform.OrthoZoom - ScrollNotches * ZoomSpeed * DeltaTime;
			ViewTransform.OrthoZoom = Clamp(NewWidth, 0.1f, 1000.0f);
		}
		else
		{
			TargetLocation += ViewTransform.ViewRotation.GetForwardVector() * (ScrollNotches * ZoomSpeed * 0.015f);
		}
	}

	ImVec2 MousePos = ImGui::GetIO().MousePos;
	float LocalMouseX = MousePos.x - ViewportScreenRect.X;
	float LocalMouseY = MousePos.y - ViewportScreenRect.Y;

	float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : 1.0f;
	float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : 1.0f;

	FMinimalViewInfo POV;
	GetCameraView(POV);
	FRay Ray = POV.DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);
	FHitResult HitResult;

	FRayUtils::RaycastComponent(Gizmo, Ray, HitResult);

	InputSystem& Input = InputSystem::Get();

	if (Input.GetKeyDown(VK_LBUTTON))
	{
		HandleDragStart(Ray);
	}
	else if (Input.GetLeftDragging())
	{
		if (Gizmo->IsPressedOnHandle() && !Gizmo->IsHolding())
		{
			Gizmo->SetHolding(true);
		}

		if (Gizmo->IsHolding())
		{
			Gizmo->UpdateDrag(Ray);
		}
	}
	else if (Input.GetLeftDragEnd())
	{
		if (Gizmo->IsHolding())
		{
			Gizmo->DragEnd();
		}
	}
	else if (Input.GetKeyUp(VK_LBUTTON))
	{
		Gizmo->SetPressedOnHandle(false);
	}
}

void FMeshEditorViewportClient::SyncCameraSmoothingTarget()
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

void FMeshEditorViewportClient::ApplySmoothedCameraLocation(float DeltaTime)
{
	const FVector CurrentLocation = ViewTransform.ViewLocation;
	const float LerpAlpha = Clamp(DeltaTime * SmoothLocationSpeed, 0.0f, 1.0f);
	const FVector NewLocation = CurrentLocation + (TargetLocation - CurrentLocation) * LerpAlpha;
	ViewTransform.ViewLocation = NewLocation;

	LastAppliedCameraLocation = NewLocation;
	bLastAppliedCameraLocationInitialized = true;
}

void FMeshEditorViewportClient::SyncGizmo()
{
	if (!Gizmo || !PreviewActor) return;

	if (const FBone* SelectedBone = GetSelectedBone())
	{
	}
	else
	{
		Gizmo->Deactivate();
	}
}

void FMeshEditorViewportClient::ApplyTransformSettingsToGizmo()
{
	if (!Gizmo) return;

	const FGizmoToolSettings& Settings = FEditorSettings::Get().MeshEditorViewportSettings.Gizmo;
	const bool bForceLocalForScale = Gizmo->GetMode() == EGizmoMode::Scale;

	Gizmo->SetWorldSpace(bForceLocalForScale ? false : Settings.CoordSystem == EEditorCoordSystem::World);
	Gizmo->SetSnapSettings(
		Settings.bEnableTranslationSnap, Settings.TranslationSnapSize,
		Settings.bEnableRotationSnap, Settings.RotationSnapSize,
		Settings.bEnableScaleSnap, Settings.ScaleSnapSize
	);
}

void FMeshEditorViewportClient::HandleDragStart(const FRay& Ray)
{
	FHitResult Hit;
	if (FRayUtils::RaycastComponent(Gizmo, Ray, Hit))
	{
		Gizmo->SetPressedOnHandle(true);
	}
}
