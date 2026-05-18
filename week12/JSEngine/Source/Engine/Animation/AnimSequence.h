#pragma once
#include "AnimTypes.h"
#include "Core/CoreMinimal.h"
#include "Object/Object.h"

using FVector3f = FVector;
using FQuat4f = FQuat;

struct FFrameRate
{
	int32 Numerator = 30;
	int32 Denominator = 1;

	float AsDecimal() const
	{
		return Denominator != 0 ? static_cast<float>(Numerator) / static_cast<float>(Denominator) : 0.0f;
	}
};

struct FRawAnimSequenceTrack
{
	TArray<FVector3f> PosKeys;
	TArray<FQuat4f> RotKeys;
	TArray<FVector3f> ScaleKeys;
};

struct FBoneAnimationTrack
{
	FName Name;
	FRawAnimSequenceTrack InternalTrack;
};

struct FAnimationCurveData
{
};

UCLASS()
class UAnimationAsset : public UObject
{
public:
	GENERATED_BODY(UAnimationAsset, UObject)

	UAnimationAsset() = default;
	~UAnimationAsset() override = default;
};

UCLASS()
class UAnimDataModel : public UObject
{
public:
	GENERATED_BODY(UAnimDataModel, UObject)

	UAnimDataModel() = default;
	~UAnimDataModel() override = default;

	virtual const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const;
	TArray<FBoneAnimationTrack>& GetMutableBoneAnimationTracks();

	float GetPlayLength() const { return PlayLength; }
	void SetPlayLength(float InPlayLength) { PlayLength = InPlayLength; }

	const FFrameRate& GetFrameRate() const { return FrameRate; }
	void SetFrameRate(const FFrameRate& InFrameRate) { FrameRate = InFrameRate; }

	int32 GetNumberOfFrames() const { return NumberOfFrames; }
	void SetNumberOfFrames(int32 InNumberOfFrames) { NumberOfFrames = InNumberOfFrames; }

	int32 GetNumberOfKeys() const { return NumberOfKeys; }
	void SetNumberOfKeys(int32 InNumberOfKeys) { NumberOfKeys = InNumberOfKeys; }

	const FAnimationCurveData& GetCurveData() const { return CurveData; }
	FAnimationCurveData& GetMutableCurveData() { return CurveData; }

private:
	TArray<FBoneAnimationTrack> BoneAnimationTracks;
	float PlayLength = 0.0f;
	FFrameRate FrameRate;
	int32 NumberOfFrames = 0;
	int32 NumberOfKeys = 0;
	FAnimationCurveData CurveData;
};

UCLASS()
class UAnimSequenceBase : public UAnimationAsset
{
public:
	GENERATED_BODY(UAnimSequenceBase, UAnimationAsset)
	virtual ~UAnimSequenceBase() = default;

	UAnimDataModel* GetDataModel() const { return DataModel; }
	void SetDataModel(UAnimDataModel* InDataModel) { DataModel = InDataModel; }

	virtual float GetPlayLength() const { return PlayLength; }
	virtual const TArray<FAnimNotifyEvent>& GetNotifies() const { return Notifies; }
	virtual const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const;
	virtual bool GetAnimationPose(float Time, FPoseContext& OutPose) const { return false; }
	void AddNotify(float InTriggerTime, const FName& InNotifyName);
	void ClearNotifies();
	bool RemoveNotifyAt(int32 NotifyIndex);

	void SetPreviewMeshPath(const FString& InPreviewMeshPath) { PreviewMeshPath = InPreviewMeshPath; }
	const FString& GetPreviewMeshPath() const { return PreviewMeshPath; }

protected:
	float PlayLength = 5.0f;
	TArray<FAnimNotifyEvent> Notifies;
	UAnimDataModel* DataModel = nullptr;
	FString PreviewMeshPath;
};

UCLASS()
class UAnimSequence : public UAnimSequenceBase
{
public:
	GENERATED_BODY(UAnimSequence, UAnimSequenceBase)
	UAnimSequence() = default;
	~UAnimSequence() override = default;

	float GetPlayLength() const override;
	bool GetAnimationPose(float Time, FPoseContext& OutPose) const override;

	void SetAssetPath(const FString& InAssetPath) { AssetPath = InAssetPath; }
	const FString& GetAssetPath() const { return AssetPath; }

	void SetSourceFilePath(const FString& InSourceFilePath) { SourceFilePath = InSourceFilePath; }
	const FString& GetSourceFilePath() const { return SourceFilePath; }

	void SetSourceStackName(const FString& InSourceStackName) { SourceStackName = InSourceStackName; }
	const FString& GetSourceStackName() const { return SourceStackName; }

	void SetSourceFileWriteTimeTicks(uint64 InSourceFileWriteTimeTicks) { SourceFileWriteTimeTicks = InSourceFileWriteTimeTicks; }
	uint64 GetSourceFileWriteTimeTicks() const { return SourceFileWriteTimeTicks; }

	void SetSourceFileSizeBytes(uint64 InSourceFileSizeBytes) { SourceFileSizeBytes = InSourceFileSizeBytes; }
	uint64 GetSourceFileSizeBytes() const { return SourceFileSizeBytes; }

	void SetSourceFileContentHash(const FString& InSourceFileContentHash) { SourceFileContentHash = InSourceFileContentHash; }
	const FString& GetSourceFileContentHash() const { return SourceFileContentHash; }

	void SetDerivedDataCachePath(const FString& InDerivedDataCachePath) { DerivedDataCachePath = InDerivedDataCachePath; }
	const FString& GetDerivedDataCachePath() const { return DerivedDataCachePath; }

	void SetDerivedDataCacheVersion(int32 InDerivedDataCacheVersion) { DerivedDataCacheVersion = InDerivedDataCacheVersion; }
	int32 GetDerivedDataCacheVersion() const { return DerivedDataCacheVersion; }

	void SetJsonTracksEmbedded(bool bInJsonTracksEmbedded) { bJsonTracksEmbedded = bInJsonTracksEmbedded; }
	bool AreJsonTracksEmbedded() const { return bJsonTracksEmbedded; }

private:
	FString AssetPath;
	FString SourceFilePath;
	FString SourceStackName;
	uint64 SourceFileWriteTimeTicks = 0;
	uint64 SourceFileSizeBytes = 0;
	FString SourceFileContentHash;
	FString DerivedDataCachePath;
	int32 DerivedDataCacheVersion = 0;
	bool bJsonTracksEmbedded = false;
};

// Debug용. 추후 삭제.
UCLASS()
class UDebugAnimSequence : public UAnimSequenceBase
{
public:
	GENERATED_BODY(UDebugAnimSequence, UAnimSequenceBase)

	float GetPlayLength() const override { return 5.0f; }
	bool GetAnimationPose(float Time, FPoseContext& OutPose) const override
	{
		if (OutPose.LocalPose.size() <= 1)
		{
			return false;
		}

		const float Angle = std::sin(Time * 6.283185f) * 30.0f;

		FMatrix Base = OutPose.LocalPose[1];
		FMatrix AnimRot = FMatrix::MakeRotationEuler(FVector(0.0f, 0.0f, Angle));

		OutPose.LocalPose[1] = AnimRot * Base;

		return true;
	}
};
