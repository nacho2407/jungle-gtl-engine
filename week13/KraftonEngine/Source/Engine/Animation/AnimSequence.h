#pragma once

#include "AnimSequenceBase.h"
#include "BoneAnimationTrack.h"

class UAnimDataModel;
class USkeletalMesh;

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
	// 균등 간격 키 가정 (key i 의 시간 = i * PlayLength / (NumKeys - 1)).
	// Looping 이면 wrap, 아니면 clamp. 키 0개 → ref pose 유지, 1개 → 상수.
	// 회전 Slerp, 위치/스케일 Lerp. 본 별 BoneTreeIndex 가 Output.Pose 의 인덱스.
	void GetBonePose(FPoseContext& Output, const FAnimExtractContext& Ctx) const override;

	// 직접 트랙 접근 (Viewer/디버그/에디터용).
	const TArray<FBoneAnimationTrack>& GetBoneTracks() const { return BoneTracks; }

	// ─────────────────────────────────────────────────────────────
	// Mock factories (A 의 FBX 임포트 전 시각 검증용 — 임시 데이터).
	// 두 팩토리 모두 UAnimDataModel 을 새로 만들고 SetDataModel 로 묶어 반환.
	// 반환된 UAnimSequence/UAnimDataModel 은 UObjectManager 가 소유 (수명 명시 관리 필요).
	// ─────────────────────────────────────────────────────────────

	// 특정 본 1개를 Z 축 기준으로 +Amp → 0 → -Amp → 0 sway 시킴. 5 키 (loop-safe).
	static UAnimSequence* CreateMockSwaySequence(
		USkeletalMesh* InMesh,
		int32 BoneIdx,
		float DurationSeconds = 1.5f,
		float AmplitudeDeg    = 30.0f);

	// 모든 본에 sinusoidal 회전. 본 인덱스로 위상차를 둬 wave 처럼 보이게.
	// TemporaryBoneAnimator 의 multi-bone 버전 재현용.
	static UAnimSequence* CreateMockWaveSequence(
		USkeletalMesh* InMesh,
		float DurationSeconds = 2.0f,
		float AmplitudeDeg    = 15.0f);

private:
	UAnimDataModel* DataModel = nullptr;
	TArray<FBoneAnimationTrack> BoneTracks; // DataModel 미사용 시 fallback
};
