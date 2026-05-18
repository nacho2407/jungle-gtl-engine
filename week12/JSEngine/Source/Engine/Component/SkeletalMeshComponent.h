#pragma once

#include "Animation/AnimGraphAsset.h"
#include "Animation/AnimSequence.h"
#include "Component/SkinnedMeshComponent.h"
#include "Core/Delegates/Delegate.h"
#include "Object/ObjectPtr.h"

struct FPoseContext;
class UAnimInstance;
class UAnimSequenceBase;
class UAnimSingleNodeInstance;
class UAnimationAsset;
class UAnimationStateMachine;
struct FAnimNotifyEvent;

UENUM()
enum class EAnimationMode
{
	AnimationBlueprint UMETA(DisplayName = "Animation Blueprint"),
	AnimationSingleNode UMETA(DisplayName = "Animation Single Node"),
	AnimationGraph UMETA(DisplayName = "Animation Graph"),
	AnimationCustomMode UMETA(DisplayName = "Animation Custom Mode")
};

/**
 * @brief Unreal Engine 스타일에서는 skinned mesh가 skeleton을 이용하는 mesh를 표현하고,
 *        skeletal mesh는 실제로 actor에 붙어서 애니메이션을 붙일 수 있는 component로 사용되고 있으므로
 *        USkeletalMeshComponent 또한 해당 방식대로 우선은 얇게 유지.
 *        핵심 로직들은 대부분 USkinnedMeshComponent로 옮겼습니다.
 */
UCLASS(SpawnableComponent, DisplayName = "SkeletalMesh Component", Category = "Basic")
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
	DECLARE_DELEGATE(FOnAnimNotify, USkeletalMeshComponent*, const FAnimNotifyEvent&)
	GENERATED_BODY(USkeletalMeshComponent, USkinnedMeshComponent)

	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override = default;

	virtual void Serialize(FArchive& Ar) override;
	virtual void PostDuplicate(UObject* Original) override;
	virtual void PostEditProperty(const char* PropertyName) override;

	void TickComponent(float DeltaTime) override;

	EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_SkeletalMesh; }

	void ResetToBindPose();

	void SetBoneLocalTransform(int32 BoneIndex, const FMatrix& NewLocalTransform);
	const FMatrix& GetBoneLocalTransform(int32 BoneIndex) const;

	FMatrix GetBoneGlobalTransform(int32 BoneIndex) const;
	void SetBoneGlobalTransform(int32 BoneIndex, const FMatrix& NewGlobalTransform);

	void SetAnimInstance(UAnimInstance* InAnimInstance) { AnimInstance = InAnimInstance; }
	UAnimInstance* GetAnimInstance() const { return AnimInstance; }

	void SetAnimationMode(EAnimationMode InAnimationMode);
	EAnimationMode GetAnimationMode() const { return AnimationMode; }

	void SetAnimGraph(UAnimGraphAsset* Graph);
	void SetAnimGraphAssetPath(const FString& Path);
	void ApplyAnimGraphFromAssetPath();
	const FString& GetAnimGraphAssetPath() const { return AnimGraphAssetPath.GetPath(); }
	void SetAnimGraphFloatParameter(const FString& Name, float Value);
	void SetAnimGraphBoolParameter(const FString& Name, bool Value);
	float GetAnimGraphFloatParameter(const FString& Name) const;
	bool GetAnimGraphBoolParameter(const FString& Name) const;

	// 애니메이션
	void PlayAnimation(UAnimationAsset* NewAnimToPlay, bool bLooping);

	void SetAnimation(UAnimationAsset* NewAnimToPlay);
	UAnimationAsset* GetAnimation() const { return AnimationToPlay; }

	void Play(bool bLooping);

	FString GetAnimationAssetPath() const;
	void Stop();
	void Pause();
	float GetPlayRate() const;
	void SetPlayRate(float InPlayRate);
	float GetAnimationPosition() const;
	float GetAnimationLength() const;
	void SetAnimationPosition(float InTime);

	bool IsPlaying() const;
	bool IsLooping() const;
	void SetLooping(bool bInLooping);

	// 노티파이 수신 - AnimInstance가 호출해줄 함수
	virtual void HandleAnimNotify(const FAnimNotifyEvent& Notify);
	void ApplyAnimationPose(const FPoseContext& PoseContext);

	// StateMachine
	UAnimationStateMachine* CreateAnimationStateMachine();
	void SetAnimationStateMachine(UAnimationStateMachine* InStateMachine);
	UAnimationStateMachine* GetAnimationStateMachine() const;

	// StateMachine Lua Binding용
	void SetAnimStateByName(const FString& StateName, float BlendTime = 0.2f);

public:
	FOnAnimNotify OnAnimNotifyDelegate;

private:
	UAnimSingleNodeInstance* EnsureSingleNodeInstance();
	void ApplyAnimationFromAssetPath();
	void SyncAnimationAssetPathFromAnimation(UAnimationAsset* Animation);

	UAnimInstance* AnimInstance = nullptr;

	TSoftObjectPtr<UAnimationAsset> AnimationAssetPath;

	UPROPERTY(DisplayName = "Animation Mode")
	EAnimationMode AnimationMode = EAnimationMode::AnimationBlueprint;

	UPROPERTY(DisplayName = "Anim Graph")
	TSoftObjectPtr<UAnimGraphAsset> AnimGraphAssetPath;

	UAnimationAsset* AnimationToPlay = nullptr;
};
