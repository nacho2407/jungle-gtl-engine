#include "EditorViewportRegistry.h"

#include <cmath>
#include "Math/MathUtility.h"

namespace
{
	bool TryGetOrthoEyeAxis(EViewportType Type, FVector& OutEyeAxis)
	{
		switch (Type)
		{
		case EViewportType::OrthoTop:
			OutEyeAxis = FVector::UpVector;
			return true;
		case EViewportType::OrthoBottom:
			OutEyeAxis = FVector::DownVector;
			return true;
		case EViewportType::OrthoLeft:
			OutEyeAxis = FVector::LeftVector;
			return true;
		case EViewportType::OrthoRight:
			OutEyeAxis = FVector::RightVector;
			return true;
		case EViewportType::OrthoFront:
			OutEyeAxis = FVector::ForwardVector;
			return true;
		case EViewportType::OrthoBack:
			OutEyeAxis = FVector::BackwardVector;
			return true;
		default:
			break;
		}

		return false;
	}
}

FEditorViewportRegistry::FEditorViewportRegistry()
{
	ResetToDefault();
}

void FEditorViewportRegistry::ResetToDefault()
{
	Viewports.resize(MAX_VIEWPORTS);

	Entries.clear();
	Entries.reserve(MAX_VIEWPORTS);

	AddEntry(0, EViewportType::Perspective, 0);
	AddEntry(1, EViewportType::OrthoTop, 1);
	AddEntry(2, EViewportType::OrthoRight, 2);
	AddEntry(3, EViewportType::OrthoFront, 3);
}

FViewport* FEditorViewportRegistry::GetViewportById(FViewportId Id)
{
	if (Id == INVALID_VIEWPORT_ID)
	{
		return nullptr;
	}

	for (FViewportEntry& Entry : Entries)
	{
		if (Entry.Id == Id && Entry.bActive && Entry.Viewport)
		{
			return Entry.Viewport;
		}
	}

	return nullptr;
}

const FViewport* FEditorViewportRegistry::GetViewportById(FViewportId Id) const
{
	if (Id == INVALID_VIEWPORT_ID)
	{
		return nullptr;
	}

	for (const FViewportEntry& Entry : Entries)
	{
		if (Entry.Id == Id && Entry.bActive && Entry.Viewport)
		{
			return Entry.Viewport;
		}
	}

	return nullptr;
}

FViewportEntry* FEditorViewportRegistry::FindEntryByType(EViewportType Type)
{
	for (FViewportEntry& Entry : Entries)
	{
		if (Entry.bActive && Entry.LocalState.ProjectionType == Type)
		{
			return &Entry;
		}
	}

	for (FViewportEntry& Entry : Entries)
	{
		if (Entry.LocalState.ProjectionType == Type)
		{
			return &Entry;
		}
	}

	return nullptr;
}

const FViewportEntry* FEditorViewportRegistry::FindEntryByType(EViewportType Type) const
{
	for (const FViewportEntry& Entry : Entries)
	{
		if (Entry.bActive && Entry.LocalState.ProjectionType == Type)
		{
			return &Entry;
		}
	}

	for (const FViewportEntry& Entry : Entries)
	{
		if (Entry.LocalState.ProjectionType == Type)
		{
			return &Entry;
		}
	}

	return nullptr;
}

FViewportEntry* FEditorViewportRegistry::FindEntryByViewportID(FViewportId ViewportId)
{
	for (FViewportEntry& Entry : Entries)
	{
		if (Entry.Id == ViewportId)
		{
			return &Entry;
		}
	}

	return nullptr;
}

const FViewportEntry* FEditorViewportRegistry::FindEntryByViewportID(FViewportId ViewportId) const
{
	for (const FViewportEntry& Entry : Entries)
	{
		if (Entry.Id == ViewportId)
		{
			return &Entry;
		}
	}

	return nullptr;
}

bool FEditorViewportRegistry::SetViewportType(FViewportId ViewportId, EViewportType NewType, const FVector* FocusPointHint)
{
	FViewportEntry* TargetEntry = FindEntryByViewportID(ViewportId);
	if (!TargetEntry)
	{
		return false;
	}

	const FViewportLocalState PreviousState = TargetEntry->LocalState;
	if (PreviousState.ProjectionType == NewType)
	{
		return true;
	}

	FViewportLocalState ConvertedState = FViewportLocalState::CreateDefault(NewType);

	// Display/show settings are viewport-level intent, so keep them across type switches.
	ConvertedState.ShowFlags = PreviousState.ShowFlags;
	ConvertedState.ViewMode = PreviousState.ViewMode;
	ConvertedState.bShowGrid = PreviousState.bShowGrid;
	ConvertedState.GridSize = PreviousState.GridSize;
	ConvertedState.LineThickness = PreviousState.LineThickness;
	ConvertedState.NearPlane = PreviousState.NearPlane;
	ConvertedState.FarPlane = PreviousState.FarPlane;

	if (NewType == EViewportType::Perspective)
	{
		// Multiple perspective viewports should start from a valid perspective camera state.
		const FViewportEntry* SeedPerspective = nullptr;
		for (const FViewportEntry& Entry : Entries)
		{
			if (Entry.Id == ViewportId || !Entry.bActive)
			{
				continue;
			}

			if (Entry.LocalState.ProjectionType == EViewportType::Perspective)
			{
				SeedPerspective = &Entry;
				break;
			}
		}

		if (!SeedPerspective)
		{
			for (const FViewportEntry& Entry : Entries)
			{
				if (Entry.Id == ViewportId)
				{
					continue;
				}

				if (Entry.LocalState.ProjectionType == EViewportType::Perspective)
				{
					SeedPerspective = &Entry;
					break;
				}
			}
		}

		if (SeedPerspective)
		{
			ConvertedState.Position = SeedPerspective->LocalState.Position;
			ConvertedState.Rotation = SeedPerspective->LocalState.Rotation;
			ConvertedState.FovY = SeedPerspective->LocalState.FovY;
			ConvertedState.NearPlane = SeedPerspective->LocalState.NearPlane;
			ConvertedState.FarPlane = SeedPerspective->LocalState.FarPlane;
		}
		else if (PreviousState.ProjectionType == EViewportType::Perspective)
		{
			ConvertedState.Position = PreviousState.Position;
			ConvertedState.Rotation = PreviousState.Rotation;
			ConvertedState.FovY = PreviousState.FovY;
			ConvertedState.NearPlane = PreviousState.NearPlane;
			ConvertedState.FarPlane = PreviousState.FarPlane;
		}
	}
	else
	{
		if (PreviousState.ProjectionType != EViewportType::Perspective)
		{
			ConvertedState.OrthoTarget = PreviousState.OrthoTarget;
			ConvertedState.OrthoZoom = PreviousState.OrthoZoom;
		}
		else
		{
			FVector FocusPoint = FVector::ZeroVector;
			bool bHasFocusPoint = false;

			if (FocusPointHint)
			{
				FocusPoint = *FocusPointHint;
				bHasFocusPoint = true;
			}

			const FVector Eye = PreviousState.Position;
			FVector Forward = PreviousState.Rotation.Vector().GetSafeNormal();
			if (Forward.IsNearlyZero())
			{
				Forward = FVector::ForwardVector;
			}

			if (!bHasFocusPoint)
			{
				FVector OrthoEyeAxis = FVector::UpVector;
				if (TryGetOrthoEyeAxis(NewType, OrthoEyeAxis))
				{
					const float Denom = FVector::DotProduct(Forward, OrthoEyeAxis);
					if (std::abs(Denom) > FMath::KindaSmallNumber)
					{
						const float T = -FVector::DotProduct(Eye, OrthoEyeAxis) / Denom;
						if (T > 0.0f)
						{
							FocusPoint = Eye + Forward * T;
							bHasFocusPoint = true;
						}
					}
				}
			}

			if (!bHasFocusPoint)
			{
				const float FallbackDistance = 100.0f;
				FocusPoint = Eye + Forward * FallbackDistance;
				bHasFocusPoint = true;
			}

			ConvertedState.OrthoTarget = FocusPoint;

			const float FocusDistance = (FocusPoint - Eye).Size();
			const float SafeDistance = (std::max)(FocusDistance, 1.0f);
			const float HalfFovRadians = FMath::DegreesToRadians(PreviousState.FovY * 0.5f);
			const float HalfHeight = SafeDistance * std::tan(HalfFovRadians);
			ConvertedState.OrthoZoom = FMath::Clamp(HalfHeight, 1.0f, 10000.0f);
		}
	}

	TargetEntry->LocalState = ConvertedState;
	return true;
}

void FEditorViewportRegistry::AddEntry(FViewportId Id, EViewportType Type, int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= static_cast<int32>(Viewports.size()))
	{
		return;
	}

	FViewportEntry Entry;
	Entry.Id = Id;
	Entry.Viewport = &Viewports[SlotIndex];
	Entry.bActive = true;
	Entry.LocalState = FViewportLocalState::CreateDefault(Type);
	Entries.push_back(Entry);
}
