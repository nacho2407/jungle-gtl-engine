#pragma once
#include "Object/Object.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"

class FArchive;

enum class ECameraShakeType : uint8
{
	Sequence,
	WaveOscillator,
};

struct FSequenceCameraShakeAssetData
{
	FString LocXCurvePath;
	FString LocYCurvePath;
	FString LocZCurvePath;

	FString PitchCurvePath;
	FString YawCurvePath;
	FString RollCurvePath;

	FString FOVCurvePath;
};

struct FWaveOscillatorCameraShakeAssetData
{
	FVector LocationAmplitude = FVector(2.0f, 2.0f, 1.0f);
	FVector LocationFrequency = FVector(12.0f, 14.0f, 10.0f);

	FRotator RotationAmplitude = FRotator(0.5f, 0.8f, 0.8f);
	FRotator RotationFrequency = FRotator(11.0f, 13.0f, 9.0f);

	float FOVAmplitude = 0.02f;
	float FOVFrequency = 8.0f;
};

class UCameraShakeAsset : public UObject
{
public:
	DECLARE_CLASS(UCameraShakeAsset, UObject)

	UCameraShakeAsset() = default;
	~UCameraShakeAsset() override;

	bool LoadFromFile(const FString& Path);
	bool SaveToFile(const FString& Path) const;
	void Serialize(FArchive& Ar) override;

	void SetSourcePath(const FString& Path) { SourcePath = Path; }
	const FString& GetSourcePath() const { return SourcePath; }

public:
	int32 Version = 1;

	ECameraShakeType ShakeType = ECameraShakeType::Sequence;

	float Duration = 0.5f;
	float BlendInTime = 0.05f;
	float BlendOutTime = 0.10f;

	bool bSingleInstance = false;

	FSequenceCameraShakeAssetData Sequence;
	FWaveOscillatorCameraShakeAssetData WaveOscillator;

private:
	FString SourcePath;
};
