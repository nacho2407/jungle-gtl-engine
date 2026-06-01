#include "Editor/Viewport/Asset/PhysicsAssetEditorViewportClient.h"

#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Physics/PhysicsAsset.h"
#include "Physics/PhysicsAssetDebugDraw.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Viewport/Viewport.h"

#include <imgui.h>
#include <cmath>

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
}

void FPhysicsAssetEditorViewportClient::ResetCameraToPreviewBounds()
{
	if (!PreviewMeshComponent)
	{
		ViewTransform.ViewLocation = FVector(-5.0f, -5.0f, 3.0f);
		ViewTransform.LookAt(FVector::ZeroVector);
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
	(void)DeltaTime;
}

void FPhysicsAssetEditorViewportClient::DrawPreviewPhysicsAsset()
{
	if (!PreviewWorld || !PhysicsAsset || !PreviewMeshComponent)
	{
		return;
	}

	DrawPhysicsAssetDebug(PreviewWorld, PhysicsAsset, PreviewMeshComponent);
}
