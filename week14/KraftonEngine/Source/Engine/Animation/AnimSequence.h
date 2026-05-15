#pragma once

#include "AnimSequenceBase.h"
#include "BoneAnimationTrack.h"

class UAnimDataModel;

// 본별 키프레임을 가진 표준 시퀀스. SkeletalMesh 의 본 계층과 1:1 매핑된 Tracks 를 갖는다.
class UAnimSequence : public UAnimSequenceBase
{
public:
	DECLARE_CLASS(UAnimSequence, UAnimSequenceBase)

	UAnimSequence() = default;
	~UAnimSequence() override = default;

	// 원본 데이터 모델 (직렬화/임포트 결과). 실행 시 GetBonePose 가 이 데이터를 샘플링.
	void SetDataModel(UAnimDataModel* InModel);
	UAnimDataModel* GetDataModel() const { return DataModel; }

	// UAnimSequenceBase:
	void GetBonePose(FPoseContext& Output, const FAnimExtractContext& Ctx) const override;

	// 직접 트랙 접근 (Viewer/디버그/에디터용).
	const TArray<FBoneAnimationTrack>& GetBoneTracks() const { return BoneTracks; }

private:
	UAnimDataModel* DataModel = nullptr;
	TArray<FBoneAnimationTrack> BoneTracks; // DataModel 미사용 시 fallback
};
