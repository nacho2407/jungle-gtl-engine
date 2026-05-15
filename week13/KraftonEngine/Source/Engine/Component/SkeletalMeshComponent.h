#pragma once

#include "SkinnedMeshComponent.h"
#include "Animation/AnimationMode.h"

class UAnimInstance;
class UAnimSequenceBase;
class UClass;

// USkinnedMeshComponent 위에 애니메이션 시스템(AnimInstance, FSM, 시퀀스 재생) 을 얹는 컴포넌트.
// Skinning/bone/material/bounds 같은 mesh-layer 상태는 USkinnedMeshComponent 가 소유한다.
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)
	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override;

	// Render access 섹션: SceneProxy
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	// Mesh 가 바뀌면 AnimInstance 도 새 SkeletalMesh 기준으로 재구성해야 하므로 override.
	void SetSkeletalMesh(USkeletalMesh* InMesh) override;

	// Animation 섹션: Mode 에 따라 AnimInstance 의 생성/파기를 컴포넌트가 책임진다.
	//   - None              : AnimInstance 미생성. BoneEdit 만 적용.
	//   - AnimationSingleNode: UAnimSingleNodeInstance 자동 생성, AnimationData 로 구동.
	//   - AnimationCustom   : AnimInstanceClass 가 가리키는 자식 클래스를 FObjectFactory 로 인스턴스화.
	void SetAnimationMode(EAnimationMode InMode);
	EAnimationMode GetAnimationMode() const { return AnimationMode; }

	// SingleNode 모드용 헬퍼. Custom 모드에선 무시 (자체 인스턴스가 자체 시퀀스를 관리).
	void SetAnimation(UAnimSequenceBase* InAsset);
	UAnimSequenceBase* GetAnimation() const { return AnimationData.AnimToPlay; }
	void SetPlayRate(float InRate);
	void SetLooping(bool bInLoop);
	void SetPlaying(bool bInPlay);
	const FSingleAnimationPlayData& GetAnimationData() const { return AnimationData; }

	// Custom 모드용. 클래스 변경 시 다음 InitializeAnimation 에서 재인스턴스화.
	void SetAnimInstanceClass(UClass* InClass);
	UClass* GetAnimInstanceClass() const { return AnimInstanceClass; }

	// 외부에서 직접 만든 인스턴스 주입 (테스트 / 특수 케이스). Mode 와 무관하게 즉시 교체.
	void SetAnimInstance(UAnimInstance* InInstance);
	UAnimInstance* GetAnimInstance() const { return AnimInstance; }

	// Mode/Class/SkeletalMesh 변경 후 일관성 재정렬. SetSkeletalMesh override 안에서 자동 호출됨.
	void InitializeAnimation();
	void ClearAnimInstance();

protected:
	// 매 프레임 AnimInstance 평가 → 결과 포즈를 BoneEditLocalMatrices 로 푸시.
	// Super::TickComponent 가 마지막에 UpdateCPUSkinning 을 1회 호출하므로
	// 본 단위 setter 대신 직접 BoneEditLocalMatrices 에 쓴다 (N회 skinning 회피).
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	void EvaluateAnimInstance(float DeltaTime);

protected:
	// Animation 런타임 상태.
	EAnimationMode           AnimationMode      = EAnimationMode::None;
	FSingleAnimationPlayData AnimationData;
	UClass*                  AnimInstanceClass  = nullptr;
	UAnimInstance*           AnimInstance       = nullptr;
};
