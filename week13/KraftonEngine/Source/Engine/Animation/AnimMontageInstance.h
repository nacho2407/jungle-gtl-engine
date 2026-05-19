#pragma once

#include "Object/Object.h"
#include "Object/FName.h"

class UAnimMontage;
class UAnimInstance;
struct FPoseContext;

// Montage 재생 상태 머신.
//   AnimInstance 가 1 개 보유 — 하나의 montage 만 동시 재생 (UE 의 default slot 모델과 유사).
//
// 라이프사이클:
//   Inactive → Play() → BlendingIn → (alpha 도달) → Playing → (section end / Stop)
//                                                            ↓
//                                                       BlendingOut → Inactive
//
// 매 프레임:
//   Tick(dt, Owner)        — SectionTime 진행, section 끝나면 chain, blend alpha 갱신
//   GetBlendWeight()       — 현재 blend 가중치 (0..1) → AnimInstance 가 base pose 와 blend
//   EvaluateMontagePose()  — 현재 시점 montage 의 본 pose 생성 (base 와 blend 는 caller 담당)

#include "Source/Engine/Animation/AnimMontageInstance.generated.h"

UCLASS()
class UAnimMontageInstance : public UObject
{
public:
    GENERATED_BODY()

    UAnimMontageInstance()           = default;
    ~UAnimMontageInstance() override = default;

    // ── 제어 API ──
    void Play(UAnimMontage* InMontage, FName StartSection, float InPlayRate, float InBlendInTime);
    void Stop(float InBlendOutTime);
    void JumpToSection(FName Name);
    void SetNextSection(FName From, FName To);

    // ── 상태 조회 ──
    bool          IsActive()       const { return State != EState::Inactive; }
    bool          IsBlendingOut()  const { return State == EState::BlendingOut; }
    UAnimMontage* GetCurrentMontage() const { return CurrentMontage; }
    FName         GetCurrentSectionName() const;
    float         GetSectionTime() const { return SectionTime; }
    float         GetBlendWeight() const;

    // ── 매 프레임 ──
    void Tick(float DeltaSeconds, UAnimInstance* Owner);
    void EvaluateMontagePose(FPoseContext& OutMontagePose);   // section + time → SourceSequence GetBonePose

private:
    void EnterBlendingIn(float InBlendInTime);
    void EnterBlendingOut(float InBlendOutTime);
    void FinishStop();
    bool AdvanceSection(UAnimInstance* Owner);   // section 끝 도달 시 다음 section 으로 이동 — 더 진행 가능하면 true

    enum class EState
    {
        Inactive,
        BlendingIn,
        Playing,
        BlendingOut
    };

    UAnimMontage* CurrentMontage      = nullptr;
    int32         CurrentSectionIndex = -1;
    float         SectionTime         = 0.0f;   // 현재 section 시작 시점 이후 경과 (sec)
    FName         PendingNextSection  = FName::None;   // SetNextSection 의 1회성 override

    EState State           = EState::Inactive;
    float  BlendAlpha      = 0.0f;
    float  BlendInTime     = 0.25f;
    float  BlendOutTime    = 0.25f;
    float  PlayRate        = 1.0f;
};
