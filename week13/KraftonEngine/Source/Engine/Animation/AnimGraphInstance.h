#pragma once

#include "Animation/AnimInstance.h"
#include "Object/SoftObjectPtr.h"

class UAnimGraphAsset;

// 자산(UAnimGraphAsset) 으로 기술된 AnimGraph 를 컴파일해 평가하는 AnimInstance.
//
// UCharacterAnimInstance 가 NativeInitializeAnimation 에서 코드로 트리를 직접 build 하는
// 반면, 이 클래스는 자산을 거쳐서 동일 결과의 트리를 만든다.
//
// 단계 D1 의 흐름:
//   1) GraphAsset 미설정 시 transient 자산을 만들어 InitializeDefault (SequencePlayer→OutputPose).
//   2) DefaultSequencePath 로 실제 AnimSequence 로드 — 비어있거나 실패 시 SequenceRef=nullptr.
//   3) 결과 sequence 를 SequencePlayer 노드의 SequenceRef 에 박음.
//   4) FAnimGraphCompiler::Compile 호출 → SetRootNode.
//      Sequence 가 nullptr 이면 SequencePlayer 가 ref pose 유지.
//
// FObjectFactory 가 USkeletalMeshComponent::AnimationCustom 경로에서 이름으로 인스턴스화
// 하므로 UCLASS/GENERATED_BODY 등록 필수.

#include "Source/Engine/Animation/AnimGraphInstance.generated.h"

UCLASS()
class UAnimGraphInstance : public UAnimInstance
{
public:
	GENERATED_BODY()
	UAnimGraphInstance() = default;
	~UAnimGraphInstance() override = default;

	void NativeInitializeAnimation() override;
	void Serialize(FArchive& Ar)    override;

	UAnimGraphAsset* GetGraphAsset() const { return GraphAsset; }
	void             SetGraphAsset(UAnimGraphAsset* InAsset) { GraphAsset = InAsset; }

	// Editor PropertyWidget 의 자산 콤보로 노출 (AssetType meta). NativeInitialize 에서 LoadAnimation.
	// "None" / empty / 로드 실패 시 SequenceRef=nullptr — SequencePlayer 가 ref pose 유지.
	UPROPERTY(Edit, Save, Category="AnimGraph", DisplayName="Default Sequence", AssetType="UAnimSequence")
	FSoftObjectPtr DefaultSequencePath = "Content/Data/hirasawa-yui/IdleWithSkin_mixamo_com.uasset";

private:
	// 자산 슬롯. 단계 D1 에선 NativeInitialize 에서 자동 생성 / 자체 보유 — 직렬화는 후속 단계.
	UAnimGraphAsset* GraphAsset = nullptr;
};
