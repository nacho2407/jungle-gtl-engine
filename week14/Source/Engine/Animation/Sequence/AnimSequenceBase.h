#pragma once

#include "Object/Object.h"
#include "Animation/Notify/AnimNotifyEvent.h"

struct FPoseContext;
struct FAnimExtractContext;

// 시퀀스류 애셋의 공통 베이스. PlayLength/Notify 등 시간축이 있는 모든 자식의 공통 인터페이스.
// 실제 본 키프레임 샘플링은 UAnimSequence 가 구현한다.

#include "Source/Engine/Animation/Sequence/AnimSequenceBase.generated.h"

UCLASS()
class UAnimSequenceBase : public UObject
{
public:
	GENERATED_BODY()
    UAnimSequenceBase()           = default;
    ~UAnimSequenceBase() override = default;

    void Serialize(FArchive& Ar) override;

    // ── 시간/길이 ──
    virtual float GetPlayLength() const
    {
        return PlayLength;
    }

    virtual float GetFrameRate() const
    {
        return FrameRate;
    }

    // ── 포즈 추출 ──
    // Output.SkeletalMesh, Output.Pose.size() 는 호출자가 이미 세팅했다고 가정.
    // 자식 구현은 본별 키프레임을 Ctx.CurrentTime 으로 샘플링해서 Output.Pose 를 채운다.
    virtual void GetBonePose(FPoseContext& Output, const FAnimExtractContext& Ctx) const
    {
        (void)Output;
        (void)Ctx;
    }

    // ── Notify ──
    const TArray<FAnimNotifyEvent>& GetNotifies() const
    {
        return Notifies;
    }

    TArray<FAnimNotifyEvent>& GetMutableNotifies()
    {
        return Notifies;
    }

protected:
    float                    PlayLength = 0.0f;  // sec
    float                    FrameRate  = 30.0f; // fps
    TArray<FAnimNotifyEvent> Notifies; 
};
