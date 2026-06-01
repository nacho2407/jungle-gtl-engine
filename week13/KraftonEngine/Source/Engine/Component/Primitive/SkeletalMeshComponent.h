#pragma once

#include "Component/Primitive/SkinnedMeshComponent.h"
#include "Animation/AnimationMode.h"
#include "Object/Ptr/SubclassOf.h"
#include "Animation/AnimInstance.h"
#include "Animation/Instance/AnimSingleNodeInstance.h"
#include "Animation/Sequence/AnimSequenceBase.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Physics/BodyInstance.h"
#include "Physics/ConstraintInstance.h"
#include "Source/Engine/Component/Primitive/SkeletalMeshComponent.generated.h"

class UAnimInstance;
class UAnimSingleNodeInstance;
class UAnimSequenceBase;
class UClass;
class ULuaAnimInstance;
struct FPoseContext;

UENUM()
enum class ERagdollSelfCollisionMode : uint8
{
    DisableAll,
    DisableParentChild,
    EnableAll
};

UENUM()
enum class ESkeletalPhysicsMode : uint8
{
    AnimationOnly,
    FullRagdoll,
    Recovering,
    PartialRagdoll,
    PhysicalAnimation
};

// SkeletalMesh 전용 render proxy만 제공하는 얇은 wrapper.
// Skinning/bone/material/bounds 상태는 모두 USkinnedMeshComponent가 소유한다.
UCLASS()
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
	GENERATED_BODY()
	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override;

    // Render access 섹션: SceneProxy
    FPrimitiveSceneProxy* CreateSceneProxy() override;

    // Mesh 가 바뀌면 AnimInstance 도 새 SkeletalMesh 기준으로 재구성해야 하므로 override.
    UFUNCTION(Callable, Category="Mesh")
    void SetSkeletalMesh(USkeletalMesh* InMesh) override;

    // SingleNode 재생 편의 API.
    UFUNCTION(Callable, Category="Animation")
    void PlayAnimation(UAnimSequenceBase* NewAnimToPlay, bool bLooping);
    UFUNCTION(Callable, Category="Animation")
    void StopAnimation();

    // Animation 섹션: Mode 에 따라 AnimInstance 의 생성/파기를 컴포넌트가 책임진다.
    //   - None              : AnimInstance 미생성. BoneEdit 만 적용.
    //   - AnimationSingleNode: UAnimSingleNodeInstance 자동 생성, AnimationData 로 구동.
    //   - AnimationCustom   : AnimInstanceClass 가 가리키는 자식 클래스를 FObjectFactory 로 인스턴스화.
    UFUNCTION(Callable, Exec, Category="Animation")
    void SetAnimationMode(EAnimationMode InMode);
    UFUNCTION(Pure, Category="Animation")
    EAnimationMode GetAnimationMode() const { return AnimationMode; }

    // SingleNode 모드용 헬퍼. Custom 모드에선 무시 (자체 인스턴스가 자체 시퀀스를 관리).
    UFUNCTION(Callable, Category="Animation")
    void SetAnimation(UAnimSequenceBase* InAsset);
    UFUNCTION(Pure, Category="Animation")
    bool CanUseAnimation(UAnimSequenceBase* InAsset) const;
    UFUNCTION(Pure, Category="Animation")
    UAnimSequenceBase* GetAnimation() const { return AnimationData.AnimToPlay.Get(); }
    UFUNCTION(Callable, Exec, Category="Animation")
    void SetPlayRate(float InRate);
    UFUNCTION(Callable, Exec, Category="Animation")
    void SetLooping(bool bInLoop);
    UFUNCTION(Callable, Exec, Category="Animation")
    void SetPlaying(bool bInPlay);
    const FSingleAnimationPlayData& GetAnimationData() const { return AnimationData; }

    // Custom 모드용. 클래스 변경 시 다음 InitializeAnimation 에서 재인스턴스화.
    // 슬롯은 TSubclassOf<UAnimInstance> — 잘못된 클래스 대입은 nullptr 로 흡수.
    UFUNCTION(Callable, Category="Animation")
    void SetAnimInstanceClass(UClass* InClass);
    UFUNCTION(Pure, Category="Animation")
    UClass* GetAnimInstanceClass() const { return AnimInstanceClass.Get(); }

    // 외부에서 직접 만든 인스턴스 주입 (테스트 / 특수 케이스). Mode 와 무관하게 즉시 교체.
    UFUNCTION(Callable, Category="Animation")
    void SetAnimInstance(UAnimInstance* InInstance);
    UFUNCTION(Pure, Category="Animation")
    UAnimInstance* GetAnimInstance() const { return AnimInstance.Get(); }

    // SingleNode 모드에서 현재 자동 생성된 노드를 반환한다. NodeName 은 현재 단일 노드 구조에서는 무시한다.
    UFUNCTION(Pure, Category="Animation")
    UAnimSingleNodeInstance* GetAnimNodeInstance(FName NodeName) const;

    //Ragdoll runtime section.
    UFUNCTION(Callable, Exec, Category="Physics|Ragdoll")
    void SetRagdollEnabled(bool bEnabled);

    UFUNCTION(Callable, Category = "Physics|Ragdoll")
    void EnablePartialRagdollBelow(FName BoneName);

    UFUNCTION(Pure, Category="Physics|Ragdoll")
    bool IsRagdollEnabled() const { return bRagdollActive; }

    UFUNCTION(Callable, Category="Physics|Ragdoll")
    void WakeAllRagdollBodies();

    UFUNCTION(Callable, Category="Physics|Ragdoll")
    void AddImpulseToBone(FName BoneName, const FVector& Impulse);

    UFUNCTION(Callable, Category = "Physics|Ragdoll")
    void SetAllBodiesPhysicsBlendWeight(float InPhysicsBlendWeight);

    UFUNCTION(Callable, Category = "Physics|Ragdoll")
    void SetAllBodiesBelowPhysicsBlendWeight(FName InBoneName, float InPhysicsBlendWeight, bool bIncludeSelf = true);

    UFUNCTION(Callable, Category = "Physics|Ragdoll")
    void SetAllBodiesSimulatePhysics(bool bSimulate);

    UFUNCTION(Callable, Category = "Physics|Ragdoll")
    void SetAllBodiesBelowSimulatePhysics(FName InBoneName, bool bSimulate, bool bIncludeSelf = true);

    UFUNCTION(Callable, Category = "Physics|PhysicalAnimation")
    bool BeginPhysicalAnimation();

    UFUNCTION(Callable, Category = "Physics|PhysicalAnimation")
    void EndPhysicalAnimation(bool bUseRecovery = true);

    UFUNCTION(Pure, Category = "Physics|PhysicalAnimation")
    bool IsPhysicalAnimationActive() const { return SkeletalPhysicsMode == ESkeletalPhysicsMode::PhysicalAnimation; }

    bool EvaluateAnimationPoseOnly(float DeltaTime, FPoseContext& OutPose);
    bool BuildWorldTransformsFromLocalPose(const TArray<FTransform>& LocalPose, TArray<FTransform>& OutWorldTransforms) const;
    void TickPhysicalAnimationPose(float DeltaTime);

    const TArray<FBodyInstance*>& GetRagdollBodies() const { return Bodies; }
    const TArray<FConstraintInstance*>& GetRagdollConstraints() const { return Constraints; }

    // Mode/Class/SkeletalMesh 변경 후 일관성 재정렬. SetSkeletalMesh override 안에서 자동 호출됨.
    void InitializeAnimation();
    void ClearAnimInstance();

    // Editor / 직렬화 통합.
    void GetEditableProperties(TArray<FPropertyValue>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;
    void PostDuplicate() override;
    void Serialize(FArchive& Ar) override;

protected:
    // 매 프레임 AnimInstance 평가 → 결과 포즈를 SetBoneLocalTransforms 로 푸시.
    // 이 경로가 CPU skinning 과 bounds dirty 를 한 번에 처리한다.
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

    void EndPlay() override;

    bool EvaluateAnimationPose(float DeltaTime, FPoseContext& OutPose);
    void ApplyAnimationPose(const FPoseContext& Pose);
    bool EvaluateAnimInstance(float DeltaTime);

    void AddReferencedObjects(FReferenceCollector& Collector) override;
private:
    void LoadAnimationFromPath();
    void CapturePersistentAnimInstanceSettings();
    void ApplyPersistentAnimInstanceSettings(UAnimInstance* Instance);

    UPhysicsAsset* GetPhysicsAssetForRagdoll();
    bool ValidatePhysicsAssetForRagdoll(UPhysicsAsset* PhysicsAsset, const FSkeletalMesh* Asset) const;

    void EnableRagdollPhysics();
    void DisableRagdollPhysics();
    void SetSkeletalPhysicsMode(ESkeletalPhysicsMode NewMode);
    void SetAllRagdollBodiesKinematic(bool bInKinematic);
    void SetAllRagdollBodiesGravityEnabled(bool bEnableGravity);
    void ForceStopRagdollWithoutRecovery();
    void SyncComponentToRagdollBody();
    FBodyInstance* FindRagdollComponentSyncBody() const;
    void CaptureRagdollComponentSyncOffset();
    void ClearRagdollComponentSyncState();
    void CacheRagdollComponentWorldMatrix();
    void ClearRagdollComponentMoveState();
    void ApplyExternalComponentMoveToRagdollBodies();
    void MoveAllRagdollBodiesByComponentDelta(const FVector& Delta);
    void TickRagdollPhysicsMode(float DeltaTime);
    void StartRagdollRecovery();
    bool TickRagdollRecovery(float DeltaTime);
    void ClearRagdollRecoveryState();
    void CaptureCurrentBoneLocalPose(TArray<FTransform>& OutPose) const;
    FTransform BlendBoneTransform(const FTransform& From, const FTransform& To, float Alpha) const;

    bool CreateRagdollBodiesFromPhysicsAsset();
    bool CreateRagdollConstraintsFromPhysicsAsset();

    bool ShouldRagdollIgnoreSameOwner() const;
    bool ShouldRagdollConstraintEnableCollision() const;

    bool BuildBodyInstanceInitDescFromBodySetup( const UBodySetup* BodySetup, int32 BoneIndex, const TArray<FMatrix>& BoneGlobals, FBodyInstanceInitDesc& OutDesc) const;

    void DestroyRagdollConstraints();
    void DestroyRagdollBodies();
    void SyncBonesFromRagdollBodies();

    bool UpdateRagdollActivePose(float DeltaTime);
    bool BuildRagdollPhysicsLocalPose( const TArray<FTransform>& SourceLocalPose, TArray<FTransform>& OutPhysicsLocalPose, TArray<float>& OutPhysicsWeights) const;
    bool BuildGlobalMatricesFromLocalPose( const TArray<FTransform>& LocalPose, TArray<FMatrix>& OutGlobalMatrices) const;
    bool ConvertGlobalMatricesToLocalPose( const TArray<FMatrix>& GlobalMatrices, const TArray<FTransform>& SourceLocalPose, TArray<FTransform>& OutLocalPose) const;
    void BlendLocalPosesByPhysicsWeight( const TArray<FTransform>& AnimationPose, const TArray<FTransform>& PhysicsPose,
        const TArray<float>& PhysicsWeights, float GlobalPhysicsWeight, TArray<FTransform>& OutBlendedPose) const;

    void EnsureRagdollPhysicsBlendWeights(float DefaultWeight = 1.0f);
    bool IsBoneBelow(int32 BoneIndex, int32 RootBoneIndex, bool bIncludeSelf) const;
    float GetRagdollPhysicsBlendWeightForBone(int32 BoneIndex) const;

    void EnsureRagdollBodySimulateFlags(bool bDefaultSimulate = true);
    void ApplyRagdollBodySimulationFlags();
    bool ShouldRagdollBodySimulate(int32 BoneIndex) const;
    void UpdateKinematicRagdollBodiesFromLocalPose(const TArray<FTransform>& SourceLocalPose);

    FBodyInstance* FindRagdollBodyByBoneIndex(int32 BoneIndex) const;
    FBodyInstance* FindRagdollBodyByBoneName(FName BoneName) const;
    int32 FindBoneIndexByName(FName BoneName) const;

protected:
    // Animation 런타임 상태.
    UPROPERTY(Edit, Save, Category="Animation", DisplayName="Animation Mode", Enum=EAnimationMode)
    EAnimationMode             AnimationMode = EAnimationMode::None;
    UPROPERTY(Edit, Save, Category="Animation", DisplayName="Animation Data", Type=Struct)
    FSingleAnimationPlayData   AnimationData;
    UPROPERTY(Edit, Save, Category="Animation", DisplayName="Anim Instance Class", Type=ClassRef, AllowedClass=UAnimInstance)
    TSubclassOf<UAnimInstance> AnimInstanceClass;
    // AnimInstance 는 runtime-owned transient 이므로 PIE duplicate/scene save 에서 LuaAnimInstance.ScriptFile 을 컴포넌트가 영속 보관한다.
    UPROPERTY(Save, Category="Animation|Lua", DisplayName="Lua Anim Script", AssetType="LuaAnimScript")
    FString LuaAnimScriptFile;
    // Runtime-owned instance. AnimInstanceClass is the persistent/editor-facing identity.
    UPROPERTY(Transient, Instanced, Category="Animation", DisplayName="Anim Instance", Type=ObjectRef, AllowedClass=UAnimInstance)
    TObjectPtr<UAnimInstance>  AnimInstance  = nullptr;

    UPROPERTY(Edit, Save, Category="Physics|Ragdoll", DisplayName="Enable Ragdoll")
    bool bRagdollEnabled = false;
    UPROPERTY(Edit, Save, Category="Physics|Ragdoll", DisplayName="Create Ragdoll Constraints")
    bool bCreateRagdollConstraints = true;
    UPROPERTY(Edit, Save, Category="Physics|Ragdoll", DisplayName="Self Collision Mode", Enum=ERagdollSelfCollisionMode)
    ERagdollSelfCollisionMode RagdollSelfCollisionMode = ERagdollSelfCollisionMode::DisableParentChild;
    UPROPERTY(Edit, Save, Category="Physics|Ragdoll", DisplayName="Ragdoll Recovery Duration")
    float RagdollRecoveryDuration = 0.35f;
    UPROPERTY(Edit, Save, Category = "Physics|Ragdoll", DisplayName = "Ragdoll Physics Blend Weight")
    float RagdollPhysicsBlendWeight = 1.0f;

    //Ragdoll runtime state
    TArray<FBodyInstance*> Bodies;
    TArray<FConstraintInstance*> Constraints;
    TArray<float> PerBoneRagdollPhysicsBlendWeights;
    TArray<bool> PerBoneRagdollBodySimulateFlags;

    ESkeletalPhysicsMode SkeletalPhysicsMode = ESkeletalPhysicsMode::AnimationOnly;
    bool bRagdollActive = false;
    int32 RagdollComponentSyncBoneIndex = -1;
    FVector RagdollComponentSyncLocalOffset = FVector::ZeroVector;
    bool bHasRagdollComponentSyncOffset = false;
    FMatrix LastRagdollComponentWorldMatrix = FMatrix::Identity;
    bool bHasLastRagdollComponentWorldMatrix = false;
    bool bRagdollRecovering = false;
    float RagdollRecoveryElapsed = 0.0f;
    TArray<FTransform> RagdollRecoveryStartLocalPose;
};
