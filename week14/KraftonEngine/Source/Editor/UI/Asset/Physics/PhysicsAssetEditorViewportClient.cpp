#include "Editor/UI/Asset/Physics/PhysicsAssetEditorViewportClient.h"
//#include "Physics/PhysicsAsset.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Debug/DrawDebugHelpers.h"
#include "Viewport/Viewport.h"
#include "Render/Types/MinimalViewInfo.h"
#include <algorithm>
#include <cmath>

static bool RayIntersectSphere(const FRay& Ray, const FVector& SphereCenter, float Radius, float& OutDistance)
{
	FVector vector = Ray.Origin - SphereCenter;
	float a = vector.Dot(Ray.Direction);
	float b = vector.Dot(vector) - (Radius * Radius);

	if (b > 0.0f && a > 0.0f)
		return false;
	float disc = (a * a) - b;
	if (disc < 0.0f)
		return false;

	float t = -a - std::sqrt(disc);
	if (t< 0.0f)
		t = 0.0f;
	OutDistance = t; 
	return true;
}
static bool RayIntersectOBB(const FRay& Ray, const FTransform& BoxWorld, const FVector& Extents, float& OutDistance)
{
	FMatrix M_InvBoxWorld = BoxWorld.ToMatrix().GetInverse();
	FVector LocalOrigin = M_InvBoxWorld.TransformVector(Ray.Origin);
	FVector LocalDir = M_InvBoxWorld.TransformVector(Ray.Direction).Normalized();

	float tMin = 0.0f;
	float tMax = FLT_MAX;

	float HalfExtents[3] = { Extents.X, Extents.Y, Extents.Z };
	float RayOri[3] = { LocalOrigin.X, LocalOrigin.Y, LocalOrigin.Z };
	float RayDir[3] = { LocalDir.X, LocalDir.Y, LocalDir.Z };
	for (int i = 0; i < 3; i++)
	{
		if (std::abs(RayDir[i]) < 1e-6f)
		{
			if (RayOri[i] < -HalfExtents[i] || RayOri[i] > HalfExtents[i])
			{
				return false;
			}	
		}
		else
		{
			float InvD = 1.0f / RayDir[i];
			float t0 = (-HalfExtents[i] - RayOri[i]) * InvD;
			float t1 = (HalfExtents[i] - RayOri[i]) * InvD;

			if (t0 > t1) std::swap(t0, t1);

			tMin = std::max(tMin, t0);
			tMax = std::min(tMax, t1);

			if (tMin > tMax) return false;
		}
	}
	OutDistance = tMin;
	return true;
}
static bool RayIntersectCapsule(const FRay& Ray, const FTransform& SphylWorld, float Radius, float Length, float& OutDistance)
{
	FMatrix M_InvSphylWorld = SphylWorld.ToMatrix().GetInverse();
	FVector LocalOrigin = M_InvSphylWorld.TransformPositionWithW(Ray.Origin);
	FVector LocalDir = M_InvSphylWorld.TransformVector(Ray.Direction).Normalized();


	float HalfHeight = Length * 0.5f;

	float tCyllinder = FLT_MAX;
}
FPhysicsAssetEditorViewportClient::FPhysicsAssetEditorViewportClient()
{
	RenderOptions.bShowPhysicsShapes = true;
	RenderOptions.bShowConstraints = true;
	RenderOptions.bShowBones = true;
}

void FPhysicsAssetEditorViewportClient::NotifyViewportResized(int32 NewWidth, int32 NewHeight)
{
	if (Viewport)
		Viewport->Resize(NewWidth, NewHeight);
}

bool FPhysicsAssetEditorViewportClient::GetCameraView(FMinimalViewInfo& OutPOV) const
{
	OutPOV.Location = FVector(0.0f, -2.0f, 1.0f);
	OutPOV.Rotation = FRotator(15.0f, 90.0f, 0.0f);
	OutPOV.FOV = 70.0f;
	return true;
}

void FPhysicsAssetEditorViewportClient::Tick(float DeltaTime)
{
	if (!PreviewWorld || !PreviewMeshComponent) return;

	if (bSimulating)
	{
		TArray<FTransform> ActivePose;
		PreviewMeshComponent->GetCurrentBoneGlobalTransforms(ActivePose);
		PreviewMeshComponent->SetBoneLocalTransforms(ActivePose);
	}
	else
	{
		PreviewMeshComponent->ApplyBoneEditBasePose();
	}

	DrawDebugPhysics(PreviewWorld, DeltaTime);
}

void FPhysicsAssetEditorViewportClient::InitializePreviewScene(UWorld* InWorld, UPhysicsAsset* InAsset, USkeletalMeshComponent* InComponent)
{
	PreviewWorld = InWorld;
	EditedPhysicsAsset = InAsset;
	PreviewMeshComponent = InComponent;
}

void FPhysicsAssetEditorViewportClient::SetSimulationEnabled(bool bEnable)
{
}

void FPhysicsAssetEditorViewportClient::DrawDebugPhysics(UWorld* World, float DeltaTime)
{
}

void FPhysicsAssetEditorViewportClient::DrawDebugJointsAndLimits(UWorld* World)
{
}

void FPhysicsAssetEditorViewportClient::DrawCollisionExclusionLinkers(UWorld* World)
{
}

FShapeIntersectionResult FPhysicsAssetEditorViewportClient::CastSelectionRay(const FRay& Ray)
{
	return FShapeIntersectionResult();
}

void* FPhysicsAssetEditorViewportClient::GetRenderTargetTexture() const
{
	return Viewport ? Viewport->GetSRV() : nullptr;
}
