#pragma once

#include "Editor/Viewport/EditorViewportClient.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Camera/ViewportCamera.h"

#include <cmath>

class FEditorMainPanelPlacementHelpers
{
public:
	static const char* GetViewportSlotName(int32 Index)
	{
		switch (Index)
		{
		case 0: return "Viewport 0";
		case 1: return "Viewport 1";
		case 2: return "Viewport 2";
		case 3: return "Viewport 3";
		default: return "Viewport";
		}
	}

	static FVector ComputePlacementLocation(FEditorViewportClient* Client, float LocalX, float LocalY)
	{
		if (!Client)
		{
			return FVector(0.0f, 0.0f, 0.0f);
		}

		FViewportCamera* Camera = Client->GetCamera();
		const FSceneViewport* Viewport = Client->GetViewport();
		if (!Camera || !Viewport)
		{
			return FVector(0.0f, 0.0f, 0.0f);
		}

		const FViewportRect& Rect = Viewport->GetRect();
		if (Rect.Width <= 0 || Rect.Height <= 0)
		{
			return Camera->GetLocation() + Camera->GetEffectiveForward().GetSafeNormal() * 10.0f;
		}

		const FRay Ray = Camera->DeprojectScreenToWorld(
			LocalX,
			LocalY,
			static_cast<float>(Rect.Width),
			static_cast<float>(Rect.Height)
		);

		if (Camera->IsOrthographic())
		{
			FVector PlaneNormal = FVector::UpVector;
			switch (Client->GetViewportType())
			{
			case EVT_OrthoTop:
			case EVT_OrthoBottom:
				PlaneNormal = FVector::UpVector;
				break;
			case EVT_OrthoFront:
			case EVT_OrthoBack:
				PlaneNormal = FVector::ForwardVector;
				break;
			case EVT_OrthoLeft:
			case EVT_OrthoRight:
				PlaneNormal = FVector::RightVector;
				break;
			default:
				PlaneNormal = Camera->GetEffectiveForward().GetSafeNormal();
				break;
			}

			const float Denom = FVector::DotProduct(Ray.Direction, PlaneNormal);
			if (std::fabs(Denom) > 0.0001f)
			{
				const float T = FVector::DotProduct(-Ray.Origin, PlaneNormal) / Denom;
				if (T >= 0.0f)
				{
					return Ray.Origin + Ray.Direction * T;
				}
			}

			FVector Projected = Ray.Origin;
			switch (Client->GetViewportType())
			{
			case EVT_OrthoTop:
			case EVT_OrthoBottom:
				Projected.Z = 0.0f;
				break;
			case EVT_OrthoFront:
			case EVT_OrthoBack:
				Projected.X = 0.0f;
				break;
			case EVT_OrthoLeft:
			case EVT_OrthoRight:
				Projected.Y = 0.0f;
				break;
			default:
				break;
			}
			return Projected;
		}

		const FVector Forward = Camera->GetEffectiveForward().GetSafeNormal();
		const FVector PlanePoint = Camera->GetLocation() + Forward * 10.0f;
		const float Denom = FVector::DotProduct(Ray.Direction, Forward);
		if (std::fabs(Denom) > 0.0001f)
		{
			const float T = FVector::DotProduct(PlanePoint - Ray.Origin, Forward) / Denom;
			if (T > 0.0f)
			{
				return Ray.Origin + Ray.Direction * T;
			}
		}

		return PlanePoint;
	}
};
