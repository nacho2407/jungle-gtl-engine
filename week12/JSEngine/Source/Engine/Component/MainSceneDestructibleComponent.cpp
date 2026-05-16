#include "Component/MainSceneDestructibleComponent.h"

#include "GameFramework/PrimitiveActors.h"
#include "Core/Debug.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace
{
std::mt19937& GetMainSceneDestructibleRng()
{
	static std::mt19937 Rng(std::random_device{}());
	return Rng;
}

float RandomRange(float Min, float Max)
{
	std::uniform_real_distribution<float> Distribution(Min, Max);
	return Distribution(GetMainSceneDestructibleRng());
}

FVector RandomUnitVector()
{
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		const FVector Candidate(
			RandomRange(-1.0f, 1.0f),
			RandomRange(-1.0f, 1.0f),
			RandomRange(-1.0f, 1.0f));

		if (Candidate.SizeSquared() > 0.0001f)
		{
			return Candidate.GetSafeNormal();
		}
	}

	return FVector::UpVector;
}
}

DEFINE_CLASS(UMainSceneDestructibleComponent, UActorComponent)
REGISTER_FACTORY(UMainSceneDestructibleComponent)

void UMainSceneDestructibleComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	bSlicePending = bAutoStart;
	bSliced = false;
	bPresentationTriggerConsumed = false;
	PresentationTrigger = 0.0f;
	Elapsed = 0.0f;
	PatrolElapsed = 0.0f;
	Fragments.clear();
	FragmentStartLocations.clear();
	FragmentTargetLocations.clear();
}

bool UMainSceneDestructibleComponent::StartSlice()
{
	AMainSceneDestructibleActor* Destructible = Cast<AMainSceneDestructibleActor>(GetOwner());
	if (!Destructible)
	{
		return false;
	}

	TArray<AMainSceneDestructibleActor*> ActiveFragments;
	ActiveFragments.push_back(Destructible);

	const int32 RequestedSliceCount = std::max(1, SliceCount);
	int32 CompletedSliceCount = 0;
	for (int32 SliceIndex = 0; SliceIndex < RequestedSliceCount && !ActiveFragments.empty(); ++SliceIndex)
	{
		std::uniform_int_distribution<size_t> IndexDistribution(0, ActiveFragments.size() - 1);
		const size_t TargetIndex = IndexDistribution(GetMainSceneDestructibleRng());
		AMainSceneDestructibleActor* Target = ActiveFragments[TargetIndex];
		if (!Target || Target->IsPendingKill())
		{
			ActiveFragments.erase(ActiveFragments.begin() + TargetIndex);
			--SliceIndex;
			continue;
		}

		TArray<AMainSceneDestructibleActor*> NewFragments;
		for (int32 Attempt = 0; Attempt < 5 && NewFragments.empty(); ++Attempt)
		{
			NewFragments = Target->SliceForMainScene(
				Target->GetActorLocation(),
				RandomUnitVector(),
				0.0f);
		}

		if (NewFragments.empty())
		{
			continue;
		}

		Target->StopPresentationMotion();
		Target->SetVisible(false);
		ActiveFragments.erase(ActiveFragments.begin() + TargetIndex);
		for (AMainSceneDestructibleActor* Fragment : NewFragments)
		{
			if (Fragment)
			{
				ActiveFragments.push_back(Fragment);
			}
		}
		++CompletedSliceCount;
	}

	Fragments = ActiveFragments;

	if (CompletedSliceCount == 0 || Fragments.empty())
	{
		UE_LOG_WARNING("[MainSceneDestructible] Slice failed: %s", Destructible->GetName().c_str());
		return false;
	}

	FragmentStartLocations.clear();
	FragmentTargetLocations.clear();
	const float DirectionDistance = SliceSpeed * SliceDuration;
	for (size_t Index = 0; Index < Fragments.size(); ++Index)
	{
		AMainSceneDestructibleActor* Fragment = Fragments[Index];
		if (!Fragment)
		{
			continue;
		}

		Fragment->StopPresentationMotion();
		const FVector StartLocation = Fragment->GetActorLocation();
		const FVector ScatterDirection = RandomUnitVector();
		const float DistanceScale = RandomRange(0.65f, 1.15f);
		FragmentStartLocations.push_back(StartLocation);
		FragmentTargetLocations.push_back(StartLocation + ScatterDirection * DirectionDistance * DistanceScale);
	}

	bSliced = true;
	Elapsed = 0.0f;
	PatrolElapsed = 0.0f;
	UE_LOG("[MainSceneDestructible] Slice started: %s Cuts=%d Fragments=%d", Destructible->GetName().c_str(), CompletedSliceCount, static_cast<int32>(Fragments.size()));
	return true;
}

void UMainSceneDestructibleComponent::Serialize(FArchive& Ar)
{
	UActorComponent::Serialize(Ar);

	if (Ar.IsLoading()
		&& !bAutoStart
		&& SliceDuration <= 0.0f
		&& SliceSpeed <= 0.0f
		&& PatrolAmplitude <= 0.0f
		&& PatrolSpeed <= 0.0f)
	{
		bAutoStart = true;
		SliceDuration = 1.0f;
		SliceSpeed = 1.2f;
		PatrolAmplitude = 0.18f;
		PatrolSpeed = 1.15f;
	}

	if (Ar.IsLoading() && SliceCount <= 0)
	{
		SliceCount = 5;
	}
}

float UMainSceneDestructibleComponent::GetRealDeltaTime(float DeltaTime) const
{
	return DeltaTime;
}

void UMainSceneDestructibleComponent::TickComponent(float DeltaTime)
{
	if (!bPresentationTriggerConsumed && PresentationTrigger >= 0.5f)
	{
		bPresentationTriggerConsumed = true;
		bSlicePending = true;
	}

	if (bSlicePending)
	{
		bSlicePending = false;
		StartSlice();
	}

	if (!bSliced)
	{
		return;
	}

	const float RealDeltaTime = GetRealDeltaTime(DeltaTime);
	Elapsed += RealDeltaTime;

	if (Elapsed < SliceDuration)
	{
		const float Alpha = SliceDuration > 0.0f ? Elapsed / SliceDuration : 1.0f;
		const float EaseOut = 1.0f - (1.0f - Alpha) * (1.0f - Alpha);

		for (size_t Index = 0; Index < Fragments.size() && Index < FragmentStartLocations.size() && Index < FragmentTargetLocations.size(); ++Index)
		{
			AMainSceneDestructibleActor* Fragment = Fragments[Index];
			if (!Fragment || Fragment->IsPendingKill())
			{
				continue;
			}

			const FVector Location = FragmentStartLocations[Index] + (FragmentTargetLocations[Index] - FragmentStartLocations[Index]) * EaseOut;
			Fragment->SetActorLocation(Location);
		}
		return;
	}

	PatrolElapsed += RealDeltaTime;
	const float Offset = std::sin(PatrolElapsed * PatrolSpeed) * PatrolAmplitude;

	for (size_t Index = 0; Index < Fragments.size() && Index < FragmentTargetLocations.size(); ++Index)
	{
		AMainSceneDestructibleActor* Fragment = Fragments[Index];
		if (!Fragment || Fragment->IsPendingKill())
		{
			continue;
		}

		const float Direction = (Index % 2 == 0) ? 1.0f : -1.0f;
		Fragment->SetActorLocation(FragmentTargetLocations[Index] + FVector::UpVector * Offset * Direction);
	}
}
