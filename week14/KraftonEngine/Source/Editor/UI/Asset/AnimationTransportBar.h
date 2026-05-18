#pragma once

class UAnimSingleNodeInstance;
class USkeletalMeshComponent;

// 언리얼 애니메이션 에디터 스타일의 아이콘 기반 트랜스포트 바.
// 재생/일시정지/역재생/프레임 스텝/처음·끝 이동/루프/속도 컨트롤을 한 줄로 렌더링한다.
namespace FAnimationTransportBar
{
	void Render(UAnimSingleNodeInstance* NodeInst,
	            USkeletalMeshComponent* Comp,
	            float TotalLength,
	            int TotalFrames);
}
