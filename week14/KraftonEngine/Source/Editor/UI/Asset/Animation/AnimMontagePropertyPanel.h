#pragma once

class UAnimMontage;
class UAnimInstance;
class USkeletalMeshComponent;

// UAnimMontage 의 편집 가능한 property + 섹션 인라인 편집 + preview 컨트롤.
// Animation tab 의 한 영역으로 호출.
//   Source sequence 변경
//   BlendIn / BlendOut 시간
//   Sections (Name, StartTime, LinkTime, NextSectionName) — 추가/삭제
//   Preview: Play (특정 section 부터), JumpToSection, Stop, 상태 표시
//
// 모든 편집은 즉시 .uasset 에 재 저장.
// Preview 호출은 owning AnimInstance 를 통해 PlayMontage / Montage_JumpToSection / StopMontage 사용.
namespace FAnimMontagePropertyPanel
{
	void Render(UAnimMontage* Montage,
	            USkeletalMeshComponent* PreviewComp = nullptr,
	            UAnimInstance* PreviewAnimInstance = nullptr);
}
