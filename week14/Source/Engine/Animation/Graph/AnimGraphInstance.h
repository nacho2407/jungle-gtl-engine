#pragma once

#include "Animation/AnimInstance.h"
#include "Object/Ptr/SoftObjectPtr.h"

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

#include "Source/Engine/Animation/Graph/AnimGraphInstance.generated.h"

UCLASS()
class UAnimGraphInstance : public UAnimInstance
{
public:
	GENERATED_BODY()
	UAnimGraphInstance() = default;
	~UAnimGraphInstance() override = default;

	void NativeInitializeAnimation()                override;
	void NativeUpdateAnimation(float DeltaSeconds)  override;
	void Serialize(FArchive& Ar)                    override;

	UAnimGraphAsset* GetGraphAsset() const { return GraphAsset; }
	void             SetGraphAsset(UAnimGraphAsset* InAsset) { GraphAsset = InAsset; }

	// Editor PropertyWidget 의 자산 콤보로 노출 (AssetType meta). NativeInitialize 에서 LoadAnimation.
	// "None" / empty / 로드 실패 시 SequenceRef=nullptr — SequencePlayer 가 ref pose 유지.
	UPROPERTY(Edit, Save, Category="AnimGraph", DisplayName="Default Sequence", AssetType="UAnimSequence")
	FSoftObjectPtr DefaultSequencePath = "Content/Data/hirasawa-yui/IdleWithSkin_mixamo_com.uasset";

	// 디스크의 UAnimGraphAsset 패키지. 비어있으면 NativeInitialize 가 transient 자산을 생성 +
	// InitializeDefault 호출 (기존 데모 흐름 유지). path 가 있으면 FAnimGraphManager 로 로드 후
	// 그 자산을 컴파일러에 넘긴다 — Editor 에서 만든 그래프가 실제 캐릭터 평가에 박힘.
	UPROPERTY(Edit, Save, Category="AnimGraph", DisplayName="Graph Asset", AssetType="UAnimGraphAsset")
	FSoftObjectPtr GraphAssetPath = "None";

private:
	// GraphAsset.Version != CompiledAssetVersion 이면 트리 폐기 + 재컴파일 + 버전 캡쳐.
	// NativeInitialize / NativeUpdate 양쪽에서 호출 — 동일 코드 경로로 첫 컴파일 / live preview 처리.
	void RecompileTreeIfDirty();

	// 자산 슬롯. GraphAssetPath 로 로드된 디스크 자산 또는 자동 생성된 transient 자산.
	UAnimGraphAsset* GraphAsset = nullptr;

	// 마지막으로 컴파일했을 때 캡쳐한 GraphAsset.Version. 매 NativeUpdate 시 현재 Version 과
	// 비교 → 다르면 OwnedNodes clear + 재컴파일 + SetRootNode + 캡쳐 갱신. in-editor live preview.
	uint32 CompiledAssetVersion = 0;
};
