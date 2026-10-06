#pragma once

#include "Core/Types/CoreTypes.h"

class UAnimSingleNodeInstance;
class USkeletalMeshComponent;
class UAnimSequence;
class USkeletalMesh;

// 언리얼 Persona 의 하단 시퀀서 패널을 모사한다.
// 프레임 눈금 룰러 + 접이식 트랙 행(Notifies/Curves/Additive/Attributes) +
// 드래그 가능한 플레이헤드 + 프레임 입력 필드가 붙은 트랜스포트 바.
//
// Notify 선택 상태는 호출자가 보유 (InOutSelectedNotifyIndex). 클릭/추가/삭제 시 동기화.
// 좌상단 AssetDetails 패널이 같은 인덱스를 읽어 RenderNotifyDetails 로 UPROPERTY 편집.
namespace FAnimationTimelinePanel
{
	void Render(UAnimSingleNodeInstance* NodeInst,
	            USkeletalMeshComponent*  Comp,
	            UAnimSequence*           Seq,
	            float                    PanelHeight,
	            int32&                   InOutSelectedNotifyIndex,
	            int32&                   InOutSelectedMorphCurveIndex,
	            int32&                   InOutSelectedMorphKeyIndex
	            );
 
	// 선택된 Notify entry 의 UPROPERTY(Edit) 편집 UI — 좌상단 AssetDetails 패널에서 호출.
	// Seq 또는 인덱스가 유효하지 않으면 안내 텍스트만 그림 (false 반환). 편집 발생 시 true.
	bool RenderNotifyDetails(UAnimSequence* Seq, int32 SelectedNotifyIndex);
	bool RenderMorphDetails(UAnimSequence* Seq, USkeletalMesh* SkeletalMesh, int32& InOutSelectedMorphCurveIndex, int32& InOutSelectedMorphKeyIndex);
}
