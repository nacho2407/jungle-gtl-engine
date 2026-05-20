#include "AnimMontage.h"

#include "Animation/Sequence/AnimSequence.h"
#include "Animation/AnimExtractContext.h"
#include "Animation/AnimationManager.h"
#include "Animation/PoseContext.h"

FArchive& operator<<(FArchive& Ar, FCompositeSection& S)
{
    Ar << S.SectionName;
    Ar << S.StartTime;
    Ar << S.LinkTime;
    Ar << S.NextSectionName;
    return Ar;
}

void UAnimMontage::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    Ar << AssetPathFileName;
    Ar << BlendInTime;
    Ar << BlendOutTime;

    // SourceSequence 는 path 로만 저장. Load 시 LoadAnimation 으로 resolve.
    if (Ar.IsSaving())
    {
        SourceSequencePath = SourceSequence ? SourceSequence->GetAssetPathFileName() : FString("None");
    }
    Ar << SourceSequencePath;

    // Sections — TArray serialize 직접 (FCompositeSection 의 operator<< 사용).
    {
        int32 Count = static_cast<int32>(Sections.size());
        Ar << Count;
        if (Ar.IsLoading())
        {
            Sections.clear();
            Sections.resize(Count);
        }
        for (int32 i = 0; i < Count; ++i)
        {
            Ar << Sections[i];
        }
    }

    // Notify 는 base 의 TArray<FAnimNotifyEvent> 사용 — montage v1 에서는 notify 직접 트래킹 안 함.
    // (SourceSequence 의 notify 가 그대로 dispatch 됨, MontageInstance 의 AddAnimNotifies 호출로.)

    // Load 후 SourceSequence resolve — caller 측 (FAnimationManager::LoadMontage) 에서 처리.
    // Serialize 안에서 LoadAnimation 호출하면 montage cache miss 시 무한루프 위험.
}

void UAnimMontage::SetSourceSequence(UAnimSequence* InSeq)
{
    SourceSequence = InSeq;
    if (InSeq)
    {
        PlayLength         = InSeq->GetPlayLength();
        FrameRate          = InSeq->GetFrameRate();
        SourceSequencePath = InSeq->GetAssetPathFileName();
        EnsureDefaultSection();
    }
    else
    {
        SourceSequencePath = "None";
    }
}

void UAnimMontage::EnsureDefaultSection()
{
    if (!Sections.empty()) return;
    FCompositeSection Default;
    Default.SectionName     = FName("Default");
    Default.StartTime       = 0.0f;
    Default.LinkTime        = SourceSequence ? SourceSequence->GetPlayLength() : 0.0f;
    Default.NextSectionName = FName::None;
    Sections.push_back(Default);
}

const FCompositeSection* UAnimMontage::FindSection(FName Name) const
{
    for (const FCompositeSection& S : Sections)
    {
        if (S.SectionName == Name) return &S;
    }
    return nullptr;
}

int32 UAnimMontage::GetSectionIndex(FName Name) const
{
    for (int32 i = 0; i < static_cast<int32>(Sections.size()); ++i)
    {
        if (Sections[i].SectionName == Name) return i;
    }
    return -1;
}

void UAnimMontage::GetBonePose(FPoseContext& Output, const FAnimExtractContext& Ctx) const
{
    if (SourceSequence)
    {
        // SourceSequence 가 위임 — Ctx.CurrentTime 은 sequence 시간 그대로.
        // Montage v1 에서는 source 의 ForceRootLock/RootMotion 옵션이 그대로 적용됨.
        SourceSequence->GetBonePose(Output, Ctx);
    }
    else
    {
        Output.ResetToRefPose();
    }
}
