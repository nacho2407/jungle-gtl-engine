#pragma once

class UAnimSingleNodeInstance;
class USkeletalMeshComponent;
class UAnimSequence;

// 언리얼 Persona 의 하단 시퀀서 패널을 모사한다.
// 프레임 눈금 룰러 + 접이식 트랙 행(Notifies/Curves/Additive/Attributes) +
// 드래그 가능한 플레이헤드 + 프레임 입력 필드가 붙은 트랜스포트 바.
namespace FAnimationTimelinePanel
{
	void Render(UAnimSingleNodeInstance* NodeInst,
	            USkeletalMeshComponent* Comp,
	            UAnimSequence* Seq,
	            float PanelHeight);
}
