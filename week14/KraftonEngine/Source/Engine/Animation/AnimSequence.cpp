#include "AnimSequence.h"
#include "AnimDataModel.h"
#include "AnimNotify_LogMessage.h"
#include "PoseContext.h"
#include "AnimExtractContext.h"
#include "AnimationRuntime.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Math/MathUtils.h"

#include <algorithm>
#include <cmath>

DEFINE_CLASS(UAnimSequence, UAnimSequenceBase)

namespace
{
	// 균등 간격 키 가정. NormalizedTime ∈ [0, 1] 을 (i0, i1, frac) 로 변환.
	// 키가 N 개면 키 i 의 시간 = i / (N - 1). 마지막 키 == time 1.0.
	void ResolveKeyIndices(int32 NumKeys, float NormalizedTime, int32& I0, int32& I1, float& Frac)
	{
		if (NumKeys <= 1)
		{
			I0 = I1 = 0;
			Frac = 0.0f;
			return;
		}
		const float Idx = NormalizedTime * static_cast<float>(NumKeys - 1);
		I0 = std::min(static_cast<int32>(std::floor(Idx)), NumKeys - 1);
		I1 = std::min(I0 + 1, NumKeys - 1);
		Frac = Idx - static_cast<float>(I0);
	}

	FVector SampleVector(const TArray<FVector>& Keys, float NormalizedTime, const FVector& Default)
	{
		if (Keys.empty()) return Default;
		if (Keys.size() == 1) return Keys[0];
		int32 I0, I1;
		float Frac;
		ResolveKeyIndices(static_cast<int32>(Keys.size()), NormalizedTime, I0, I1, Frac);
		return Keys[I0] + (Keys[I1] - Keys[I0]) * Frac;
	}

	FQuat SampleQuat(const TArray<FQuat>& Keys, float NormalizedTime, const FQuat& Default)
	{
		if (Keys.empty()) return Default;
		if (Keys.size() == 1) return Keys[0];
		int32 I0, I1;
		float Frac;
		ResolveKeyIndices(static_cast<int32>(Keys.size()), NormalizedTime, I0, I1, Frac);
		return FQuat::Slerp(Keys[I0], Keys[I1], Frac);
	}
}

void UAnimSequence::SetDataModel(UAnimDataModel* InModel)
{
	DataModel = InModel;
	if (DataModel)
	{
		PlayLength = DataModel->PlayLength;
		FrameRate  = DataModel->FrameRate;
		Notifies   = DataModel->Notifies;
		BoneTracks = DataModel->BoneAnimationTracks;
	}
}

void UAnimSequence::GetBonePose(FPoseContext& Output, const FAnimExtractContext& Ctx) const
{
	const float Length = GetPlayLength();
	if (Length <= 0.0f || BoneTracks.empty()) return;

	float Time = Ctx.CurrentTime;
	if (Ctx.bLooping)
	{
		Time = std::fmod(Time, Length);
		if (Time < 0.0f) Time += Length;
	}
	else
	{
		if (Time < 0.0f)    Time = 0.0f;
		if (Time > Length)  Time = Length;
	}

	const float NormalizedTime = Time / Length;

	for (const FBoneAnimationTrack& Track : BoneTracks)
	{
		const int32 Idx = Track.BoneTreeIndex;
		if (Idx < 0 || Idx >= static_cast<int32>(Output.Pose.size())) continue;

		FTransform& Out = Output.Pose[Idx];
		Out.Location = SampleVector(Track.InternalTrackData.PosKeys,   NormalizedTime, Out.Location);
		Out.Rotation = SampleQuat  (Track.InternalTrackData.RotKeys,   NormalizedTime, Out.Rotation);
		Out.Scale    = SampleVector(Track.InternalTrackData.ScaleKeys, NormalizedTime, Out.Scale);
	}
}

// ──────────────────────────────────────────────
// Mock factories
// ──────────────────────────────────────────────
UAnimSequence* UAnimSequence::CreateMockSwaySequence(
	USkeletalMesh* InMesh, int32 BoneIdx, float DurationSeconds, float AmplitudeDeg)
{
	if (!InMesh) return nullptr;
	FSkeletalMesh* Asset = InMesh->GetSkeletalMeshAsset();
	if (!Asset) return nullptr;
	if (BoneIdx < 0 || BoneIdx >= static_cast<int32>(Asset->Bones.size())) return nullptr;

	const FTransform Base = FAnimationRuntime::DecomposeMatrix(Asset->Bones[BoneIdx].LocalMatrix);

	const float Rad   = AmplitudeDeg * FMath::DegToRad;
	const FQuat RotP  = FQuat::FromAxisAngle(FVector(0.0f, 0.0f, 1.0f),  Rad);
	const FQuat RotN  = FQuat::FromAxisAngle(FVector(0.0f, 0.0f, 1.0f), -Rad);

	UAnimDataModel* Model = UObjectManager::Get().CreateObject<UAnimDataModel>();
	Model->PlayLength = DurationSeconds;
	Model->FrameRate  = 30.0f;
	Model->NumFrames  = 5;

	FBoneAnimationTrack Track;
	Track.BoneTreeIndex = BoneIdx;
	Track.InternalTrackData.PosKeys   = TArray<FVector>(5, Base.Location);
	Track.InternalTrackData.ScaleKeys = TArray<FVector>(5, Base.Scale);
	Track.InternalTrackData.RotKeys   = {
		Base.Rotation,
		RotP * Base.Rotation,
		Base.Rotation,
		RotN * Base.Rotation,
		Base.Rotation,
	};
	Model->BoneAnimationTracks.push_back(Track);

	UAnimSequence* Seq = UObjectManager::Get().CreateObject<UAnimSequence>();
	Seq->SetDataModel(Model);
	return Seq;
}

UAnimSequence* UAnimSequence::CreateMockWaveSequence(
	USkeletalMesh* InMesh, float DurationSeconds, float AmplitudeDeg)
{
	if (!InMesh) return nullptr;
	FSkeletalMesh* Asset = InMesh->GetSkeletalMeshAsset();
	if (!Asset || Asset->Bones.empty()) return nullptr;

	const int32 BoneCount = static_cast<int32>(Asset->Bones.size());
	const int32 KeyCount  = 9;   // 8 segments, last == first 위상으로 loop-safe
	const float Rad       = AmplitudeDeg * FMath::DegToRad;

	UAnimDataModel* Model = UObjectManager::Get().CreateObject<UAnimDataModel>();
	Model->PlayLength = DurationSeconds;
	Model->FrameRate  = 30.0f;
	Model->NumFrames  = KeyCount;

	for (int32 b = 0; b < BoneCount; ++b)
	{
		const FTransform Base = FAnimationRuntime::DecomposeMatrix(Asset->Bones[b].LocalMatrix);

		// 본 인덱스 별로 위상 차를 줘서 chain 처럼 진행. 한 사이클이 전체 본을 한 바퀴.
		const float PhaseOffset = (static_cast<float>(b) * 2.0f * FMath::Pi)
		                        / static_cast<float>(BoneCount);

		FBoneAnimationTrack Track;
		Track.BoneTreeIndex = b;
		Track.InternalTrackData.PosKeys   = TArray<FVector>(KeyCount, Base.Location);
		Track.InternalTrackData.ScaleKeys = TArray<FVector>(KeyCount, Base.Scale);
		Track.InternalTrackData.RotKeys.reserve(KeyCount);

		for (int32 k = 0; k < KeyCount; ++k)
		{
			const float Phase = (static_cast<float>(k) * 2.0f * FMath::Pi)
			                  / static_cast<float>(KeyCount - 1) + PhaseOffset;
			const float Angle = Rad * std::sin(Phase);
			const FQuat Rot   = FQuat::FromAxisAngle(FVector(0.0f, 0.0f, 1.0f), Angle);
			Track.InternalTrackData.RotKeys.push_back(Rot * Base.Rotation);
		}

		Model->BoneAnimationTracks.push_back(Track);
	}

	// Phase 7 데모 — wave 시퀀스에 LogMessage notify 2개 박아 dispatch 경로 검증.
	// 트리거는 Duration 의 25% / 75% 지점 — 길이가 짧아도 두 번 모두 발사되는 위치.
	{
		UAnimNotify_LogMessage* N1 = UObjectManager::Get().CreateObject<UAnimNotify_LogMessage>(Model);
		N1->Message = "wave-step (early)";
		Model->Notifies.push_back({ FName("WaveStep"), DurationSeconds * 0.25f, 0.0f, N1 });

		UAnimNotify_LogMessage* N2 = UObjectManager::Get().CreateObject<UAnimNotify_LogMessage>(Model);
		N2->Message = "wave-step (late)";
		Model->Notifies.push_back({ FName("WaveStep"), DurationSeconds * 0.75f, 0.0f, N2 });
	}

	UAnimSequence* Seq = UObjectManager::Get().CreateObject<UAnimSequence>();
	Seq->SetDataModel(Model);
	return Seq;
}
