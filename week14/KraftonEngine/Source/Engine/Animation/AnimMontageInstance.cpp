#include "AnimMontageInstance.h"

#include "AnimMontage.h"
#include "AnimSequence.h"
#include "AnimInstance.h"
#include "AnimExtractContext.h"
#include "PoseContext.h"
#include "Core/Log.h"

#include <algorithm>

void UAnimMontageInstance::Play(UAnimMontage* InMontage, FName StartSection, float InPlayRate, float InBlendInTime)
{
    if (!InMontage)
    {
        UE_LOG("Montage Play: null montage.");
        return;
    }

    CurrentMontage = InMontage;
    PlayRate       = (InPlayRate > 0.0f) ? InPlayRate : 1.0f;

    // 시작 section 결정 — 지정 없으면 sections[0]. 비어있으면 EnsureDefaultSection 으로 default 1개 보장.
    if (CurrentMontage->GetSections().empty())
    {
        CurrentMontage->EnsureDefaultSection();
    }
    int32 SectionIdx = (StartSection != FName::None) ? CurrentMontage->GetSectionIndex(StartSection) : 0;
    if (SectionIdx < 0) SectionIdx = 0;
    CurrentSectionIndex = SectionIdx;
    SectionTime         = 0.0f;
    PendingNextSection  = FName::None;

    EnterBlendingIn(InBlendInTime > 0.0f ? InBlendInTime : InMontage->GetBlendInTime());
}

void UAnimMontageInstance::Stop(float InBlendOutTime)
{
    if (State == EState::Inactive) return;
    EnterBlendingOut(InBlendOutTime > 0.0f ? InBlendOutTime : (CurrentMontage ? CurrentMontage->GetBlendOutTime() : 0.25f));
}

void UAnimMontageInstance::JumpToSection(FName Name)
{
    if (!CurrentMontage) return;
    const int32 Idx = CurrentMontage->GetSectionIndex(Name);
    if (Idx < 0)
    {
        UE_LOG("Montage JumpToSection: unknown section '%s'", Name.ToString().c_str());
        return;
    }
    CurrentSectionIndex = Idx;
    SectionTime         = 0.0f;
}

void UAnimMontageInstance::SetNextSection(FName From, FName To)
{
    if (!CurrentMontage) return;
    // 현재 section 이 From 이면 다음 advance 시 To 로 이동 (1회). From 가 다른 section 이면 무시.
    const FName CurName = GetCurrentSectionName();
    if (CurName == From)
    {
        PendingNextSection = To;
    }
}

FName UAnimMontageInstance::GetCurrentSectionName() const
{
    if (!CurrentMontage) return FName::None;
    const auto& Sections = CurrentMontage->GetSections();
    if (CurrentSectionIndex < 0 || CurrentSectionIndex >= static_cast<int32>(Sections.size())) return FName::None;
    return Sections[CurrentSectionIndex].SectionName;
}

float UAnimMontageInstance::GetBlendWeight() const
{
    switch (State)
    {
    case EState::Inactive:    return 0.0f;
    case EState::BlendingIn:  return std::clamp(BlendAlpha, 0.0f, 1.0f);
    case EState::Playing:     return 1.0f;
    case EState::BlendingOut: return std::clamp(BlendAlpha, 0.0f, 1.0f);
    }
    return 0.0f;
}

void UAnimMontageInstance::EnterBlendingIn(float InBlendInTime)
{
    State       = EState::BlendingIn;
    BlendAlpha  = 0.0f;
    BlendInTime = std::max(InBlendInTime, 0.0f);
    if (BlendInTime <= 0.0f)
    {
        BlendAlpha = 1.0f;
        State      = EState::Playing;
    }
}

void UAnimMontageInstance::EnterBlendingOut(float InBlendOutTime)
{
    State        = EState::BlendingOut;
    BlendAlpha   = 1.0f;
    BlendOutTime = std::max(InBlendOutTime, 0.0f);
    if (BlendOutTime <= 0.0f)
    {
        FinishStop();
    }
}

void UAnimMontageInstance::FinishStop()
{
    State               = EState::Inactive;
    CurrentMontage      = nullptr;
    CurrentSectionIndex = -1;
    SectionTime         = 0.0f;
    BlendAlpha          = 0.0f;
    PendingNextSection  = FName::None;
}

bool UAnimMontageInstance::AdvanceSection(UAnimInstance* Owner)
{
    if (!CurrentMontage) return false;
    const auto& Sections = CurrentMontage->GetSections();
    if (CurrentSectionIndex < 0 || CurrentSectionIndex >= static_cast<int32>(Sections.size())) return false;

    const FCompositeSection& Cur = Sections[CurrentSectionIndex];

    // 다음 section 결정 — PendingNextSection 우선, 없으면 Cur.NextSectionName.
    FName NextName = (PendingNextSection != FName::None) ? PendingNextSection : Cur.NextSectionName;
    PendingNextSection = FName::None;

    if (NextName == FName::None)
    {
        // chain 종료 → BlendOut.
        EnterBlendingOut(CurrentMontage->GetBlendOutTime());
        return false;
    }

    const int32 NextIdx = CurrentMontage->GetSectionIndex(NextName);
    if (NextIdx < 0)
    {
        UE_LOG("Montage AdvanceSection: next section '%s' not found in '%s'",
            NextName.ToString().c_str(), CurrentMontage->GetName().c_str());
        EnterBlendingOut(CurrentMontage->GetBlendOutTime());
        return false;
    }

    CurrentSectionIndex = NextIdx;
    SectionTime         = 0.0f;
    (void)Owner;
    return true;
}

void UAnimMontageInstance::Tick(float DeltaSeconds, UAnimInstance* Owner)
{
    if (State == EState::Inactive || !CurrentMontage) return;

    // Blend 진행.
    if (State == EState::BlendingIn)
    {
        if (BlendInTime > 0.0f)
        {
            BlendAlpha += DeltaSeconds / BlendInTime;
            if (BlendAlpha >= 1.0f)
            {
                BlendAlpha = 1.0f;
                State      = EState::Playing;
            }
        }
        else
        {
            BlendAlpha = 1.0f;
            State      = EState::Playing;
        }
    }
    else if (State == EState::BlendingOut)
    {
        if (BlendOutTime > 0.0f)
        {
            BlendAlpha -= DeltaSeconds / BlendOutTime;
            if (BlendAlpha <= 0.0f)
            {
                FinishStop();
                return;
            }
        }
        else
        {
            FinishStop();
            return;
        }
    }

    // SectionTime 진행 (BlendingOut 중에도 시간은 흐름 — 자연스러운 fade).
    const float Step = DeltaSeconds * PlayRate;
    const auto& Sections = CurrentMontage->GetSections();
    if (CurrentSectionIndex < 0 || CurrentSectionIndex >= static_cast<int32>(Sections.size())) return;

    const FCompositeSection& Cur     = Sections[CurrentSectionIndex];
    const float              CurLen  = std::max(Cur.LinkTime - Cur.StartTime, 0.0f);

    // Notify 큐 적재 + Root motion 누적 — 둘 다 source sequence 의 시간 구간 [PrevSeqTime, NextSeqTime).
    if (Owner && CurrentMontage->GetSourceSequence() && CurLen > 0.0f)
    {
        UAnimSequence* SrcSeq       = CurrentMontage->GetSourceSequence();
        const float    PrevSeqTime  = Cur.StartTime + SectionTime;
        const float    NextSeqTime  = Cur.StartTime + std::min(SectionTime + Step, CurLen);

        Owner->AddAnimNotifies(PrevSeqTime, NextSeqTime, SrcSeq);

        // Root motion — bEnableRootMotion 켜진 sequence 만, blend weight 곱해 누적.
        // AccumulateRootMotion 측 mode 체크 (IgnoreRootMotion) 가 최종 게이트.
        // RootMotionFromMontagesOnly mode 일 때 base FSM/SingleNode 누적은 호출자 측에서 skip,
        // montage 는 mode 무관 누적 (Ignore 만 막힘).
        if (SrcSeq->GetEnableRootMotion())
        {
            const float W = GetBlendWeight();
            if (W > 0.0f)
            {
                const FTransform RawDelta = SrcSeq->ExtractRootMotion(PrevSeqTime, NextSeqTime, false);
                // Translation 은 scalar scale. Rotation 은 identity 와 quat slerp 로 부분 적용.
                FTransform Scaled;
                Scaled.Location = RawDelta.Location * W;
                Scaled.Rotation = FQuat::Slerp(FQuat::Identity, RawDelta.Rotation.GetNormalized(), W).GetNormalized();
                Owner->AccumulateRootMotion(Scaled);
            }
        }
    }

    SectionTime += Step;

    // BlendingOut 중에는 section advance 로직 건너뜀.
    //   → AdvanceSection 이 다시 EnterBlendingOut 을 호출해서 BlendAlpha 가 1.0 으로 reset 되는 버그 방지.
    //   BlendingOut 은 위 BlendAlpha 카운트다운으로만 종료된다.
    if (State == EState::BlendingOut)
    {
        SectionTime = std::min(SectionTime, CurLen);   // 마지막 frame clamp — 시각적 안정.
        return;
    }

    while (CurLen > 0.0f && SectionTime >= CurLen && State != EState::Inactive && State != EState::BlendingOut)
    {
        // section 끝났음 → advance. 다음 section 으로 이동 OR BlendOut 진입.
        if (!AdvanceSection(Owner))
        {
            // BlendOut 시작됨 — section time 은 마지막 frame 유지.
            SectionTime = CurLen;
            break;
        }
        // 새 section 으로 넘어왔으면 남은 시간만큼 다시 진행.
        SectionTime = std::max(SectionTime - CurLen, 0.0f);
        const FCompositeSection& NewCur = Sections[CurrentSectionIndex];
        const float NewLen = std::max(NewCur.LinkTime - NewCur.StartTime, 0.0f);
        if (NewLen <= 0.0f) break;
        if (SectionTime < NewLen) break;
        // 다음 iteration 에서 SectionTime >= NewLen 이면 또 advance.
    }
}

void UAnimMontageInstance::EvaluateMontagePose(FPoseContext& OutMontagePose)
{
    if (!CurrentMontage || CurrentSectionIndex < 0)
    {
        OutMontagePose.ResetToRefPose();
        return;
    }

    const auto& Sections = CurrentMontage->GetSections();
    if (CurrentSectionIndex >= static_cast<int32>(Sections.size()))
    {
        OutMontagePose.ResetToRefPose();
        return;
    }

    const FCompositeSection& Cur = Sections[CurrentSectionIndex];

    FAnimExtractContext Ctx;
    Ctx.CurrentTime = Cur.StartTime + std::min(SectionTime, std::max(Cur.LinkTime - Cur.StartTime, 0.0f));
    Ctx.bLooping    = false;   // section 내부에서는 wrap 없음 — chain 으로 처리.
    CurrentMontage->GetBonePose(OutMontagePose, Ctx);
}
