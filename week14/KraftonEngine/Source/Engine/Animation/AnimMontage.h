#pragma once

#include "AnimSequenceBase.h"
#include "Object/FName.h"

class UAnimSequence;

// Montage 의 한 section — 소스 sequence 의 시간 구간 + 다음 section 링크.
//   StartTime/LinkTime: 소스 sequence 내 시간 (sec).
//   NextSection == FName::None: 끝나면 montage 종료 (자동 BlendOut).
//   NextSection == 자기 자신: 그 section 무한 loop.
struct FCompositeSection
{
    FName SectionName    = FName::None;
    float StartTime      = 0.0f;
    float LinkTime       = 0.0f;
    FName NextSectionName = FName::None;

    friend FArchive& operator<<(FArchive& Ar, FCompositeSection& S);
};

// 섹션 기반 montage 애셋.
//   - 단일 SourceSequence 위에 sections 정의 — 시간 구간별로 이름 + chain.
//   - Slot 미지원 (v1, whole-body 만).
//   - BlendIn/Out 은 montage 단위 default; 추후 per-play override 가능.
//
// 런타임에서는 UAnimMontageInstance 가 현재 section + 경과 시간을 추적하면서
// GetBonePose 를 호출해 본 pose 를 평가. AnimInstance::EvaluateAnimation 가
// base FSM pose 와 BlendWeight 로 lerp.

#include "Source/Engine/Animation/AnimMontage.generated.h"

UCLASS()
class UAnimMontage : public UAnimSequenceBase
{
public:
    GENERATED_BODY()

    UAnimMontage()           = default;
    ~UAnimMontage() override = default;

    void Serialize(FArchive& Ar) override;

    // 본 pose 평가 — Ctx.CurrentTime 은 SOURCE SEQUENCE 의 시간으로 해석.
    // (UAnimMontageInstance 가 section + sectionTime → sequence time 으로 변환 후 호출.)
    void GetBonePose(FPoseContext& Output, const FAnimExtractContext& Ctx) const override;

    // ── Source ──
    UAnimSequence* GetSourceSequence() const { return SourceSequence; }
    void           SetSourceSequence(UAnimSequence* InSeq);   // PlayLength/FrameRate 자동 동기화

    // ── Sections ──
    const TArray<FCompositeSection>& GetSections() const { return Sections; }
    TArray<FCompositeSection>&       GetMutableSections() { return Sections; }

    const FCompositeSection* FindSection(FName Name) const;
    int32                    GetSectionIndex(FName Name) const;

    // 디폴트 섹션 1개 자동 생성 — 처음 source 가 붙을 때 호출.
    void EnsureDefaultSection();

    // ── Blend ──
    float GetBlendInTime()  const { return BlendInTime; }
    float GetBlendOutTime() const { return BlendOutTime; }
    void  SetBlendInTime(float T)  { BlendInTime = T; }
    void  SetBlendOutTime(float T) { BlendOutTime = T; }

    // ── Asset path ──
    const FString& GetAssetPathFileName() const { return AssetPathFileName; }
    void           SetAssetPathFileName(const FString& Path) { AssetPathFileName = Path; }

    // Load 후 SourceSequence 가 nullptr 인 상태에서 외부 (FAnimationManager) 가
    // path 를 조회해 LoadAnimation 후 SetSourceSequence 로 wire 할 수 있도록 노출.
    const FString& GetSourceSequencePath() const { return SourceSequencePath; }

private:
    UAnimSequence*            SourceSequence  = nullptr;
    TArray<FCompositeSection> Sections;
    float                     BlendInTime     = 0.25f;
    float                     BlendOutTime    = 0.25f;
    FString                   AssetPathFileName = "None";

    // Source 가 다른 .uasset 인 경우 path 만 저장하고 load 시 resolve.
    // SourceSequence 는 runtime pointer — 직렬화 시 SourcePath 로 대체.
    FString SourceSequencePath = "None";
};
