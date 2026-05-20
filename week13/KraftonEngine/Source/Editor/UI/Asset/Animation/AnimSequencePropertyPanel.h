#pragma once

class UAnimSequence;

// UAnimSequence 의 편집 가능한 property 들을 한 곳에 모아 표시/편집하는 위젯.
// 현재 항목:
//   - Force Root Lock (horizontal 잠금)
//   - Enable Root Motion (root 본 motion → actor transform)
//   - RootMotionBoneName (자동 감지 + 수동 override 콤보)
// 앞으로 추가될 property (loop, play rate scale, root rotation 옵션 등) 도 이 panel 에 누적.
// 변경 즉시 .uasset 에 재 저장 (기존 SourcePath 메타데이터 보존).
namespace FAnimSequencePropertyPanel
{
	void Render(UAnimSequence* Seq);
}
