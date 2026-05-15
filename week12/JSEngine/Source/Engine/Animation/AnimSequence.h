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

protected:
	float PlayLength = 2.0f;
	TArray<FAnimNotifyEvent> Notifies;
};

class UAnimSequence : public UAnimSequenceBase
{
public:
	DECLARE_CLASS(UAnimSequence, UAnimSequenceBase)
	// 임시용.
	bool GetAnimationPose(float Time, FPoseContext& OutPose) const override;
};