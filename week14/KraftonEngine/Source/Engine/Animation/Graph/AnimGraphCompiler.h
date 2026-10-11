#pragma once

#include "Core/Types/CoreTypes.h"

class UAnimGraphAsset;
class UAnimInstance;
class FAnimNode_Base;

// UAnimGraphAsset (정적 데이터 모델) → FAnimNode_* 트리 (런타임 평가 가능) 변환.
// 컴파일러는 OwningInstance.MakeNode<T> 를 호출 → 생성된 노드들의 lifetime 은 OwningInstance
// 가 단일 소유 (TArray<unique_ptr> OwnedNodes). 컴파일러 자체는 노드 lifetime 신경 X.
//
// 반환 노드는 호출자가 UAnimInstance::SetRootNode 에 박는다 — SetRootNode 가 자동으로
// FAnimNode_Root 로 wrap (RootMotion 정책 단일 진입점). 컴파일러는 OutputPose 노드의 ChildPose
// 만 반환하면 됨.
class FAnimGraphCompiler
{
public:
	// nullptr 반환 — OutputPose 노드 없음 / 다수 / 종착점 핀 미연결 / 미지원 노드 타입 등.
	// 컴파일 오류는 Log 시스템으로 흘림 (지금은 LOG_ERROR 만, 후속 단계에서 NotificationToast 연동).
	static FAnimNode_Base* Compile(const UAnimGraphAsset& Graph, UAnimInstance& OwningInstance);
};
