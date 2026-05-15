#pragma once
#include "AnimTypes.h"
#include "Core/CoreMinimal.h"
#include "Object/Object.h"

class UAnimSequenceBase : public UObject
{
public:
	DECLARE_CLASS(UAnimSequenceBase, UObject)
	virtual ~UAnimSequenceBase() = default;

	virtual float GetPlayLength() const { return PlayLength; }
	virtual const TArray<FAnimNotifyEvent>& GetNotifies() const { return Notifies; }
	virtual bool GetAnimationPose(float Time, FPoseContext& OutPose) const { return false; }
	void AddNotify(float InTriggerTime, const FName& InNotifyName);

protected:
	float PlayLength = 5.0f;
	TArray<FAnimNotifyEvent> Notifies;
};

class UAnimSequence : public UAnimSequenceBase
{
public:
	DECLARE_CLASS(UAnimSequence, UAnimSequenceBase)
	// 임시용.
	bool GetAnimationPose(float Time, FPoseContext& OutPose) const override;
};

// Debug용. 추후 삭제.
class UDebugAnimSequence : public UAnimSequenceBase
{
public:
	DECLARE_CLASS(UDebugAnimSequence, UAnimSequenceBase)

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