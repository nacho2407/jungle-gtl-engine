#include "SkeletalMeshComponent.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"

#include "Animation/AnimationManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Animation/Sequence/AnimSequenceBase.h"
#include "Animation/Instance/AnimSingleNodeInstance.h"
#include "Animation/Instance/LuaAnimInstance.h"
#include "Animation/PoseContext.h"
#include "Asset/AssetRegistry.h"
#include "Core/Logging/Log.h"
#include "GameFramework/AActor.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Object/Object.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Object/Reflection/UClass.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Serialization/Archive.h"
#include "GameFramework/World.h"
#include "Physics/IPhysicsScene.h"
#include "PhysicsEngine/PhysicsAssetManager.h"


#include <algorithm>
#include <cstring>

#include "Object/GarbageCollection.h"

namespace
{
    bool ShouldCreateRagdollShape(const FKShapeElem& ShapeElem)
    {
        return ShapeElem.GetCollisionEnabled() != ECollisionEnabled::NoCollision;
    }

    FTransform NormalizeConstraintFrame(FTransform Frame)
    {
        Frame.Scale = FVector::OneVector;
        Frame.Rotation.Normalize();
        return Frame;
    }

    FTransform MakeConstraintFrameLocal(const FTransform& JointWorld, const FTransform& BodyWorld)
    {
        FTransform LocalFrame = FTransform::FromMatrixWithScale(
            JointWorld.ToMatrix() * BodyWorld.ToMatrix().GetAffineInverse()
        );

        return NormalizeConstraintFrame(LocalFrame);
    }
}

USkeletalMeshComponent::~USkeletalMeshComponent()
{
    DestroyRagdollConstraints();
    DestroyRagdollBodies();
    ClearRagdollComponentSyncState();
    ClearRagdollComponentMoveState();
    ClearRagdollRecoveryState();
    ClearAnimInstance();
}

FPrimitiveSceneProxy* USkeletalMeshComponent::CreateSceneProxy()
{
    return new FSkeletalMeshSceneProxy(this);
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
    if (bRagdollActive || bRagdollRecovering)
    {
        ForceStopRagdollWithoutRecovery();
    }

    Super::SetSkeletalMesh(InMesh);
    // Mesh 가 바뀌면 이전 AnimInstance 가 가리키던 본 인덱스/카운트가 무의미해진다.
    // 새 SkeletalMesh 기준으로 AnimInstance 를 재인스턴스화한다.
    InitializeAnimation();
}

void USkeletalMeshComponent::PlayAnimation(UAnimSequenceBase* NewAnimToPlay, bool bLooping)
{
    SetAnimationMode(EAnimationMode::AnimationSingleNode);
    SetAnimation(NewAnimToPlay);
    SetLooping(bLooping);
    SetPlaying(NewAnimToPlay != nullptr);
}

void USkeletalMeshComponent::StopAnimation()
{
    SetAnimation(nullptr);
    SetPlaying(false);

    if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        SingleNode->SetCurrentTime(0.0f);
    }
}

// ──────────────────────────────────────────────
// Animation API
// ──────────────────────────────────────────────
void USkeletalMeshComponent::SetAnimationMode(EAnimationMode InMode)
{
    if (AnimationMode == InMode) return;
    AnimationMode = InMode;
    InitializeAnimation();
}

bool USkeletalMeshComponent::CanUseAnimation(UAnimSequenceBase* InAsset) const
{
    if (!InAsset)
    {
        return true;
    }

    const USkeletalMesh* Mesh = GetSkeletalMesh();
    if (!Mesh)
    {
        return false;
    }

    if (const UAnimSequence* Sequence = Cast<UAnimSequence>(InAsset))
    {
        FSkeletonCompatibilityReport Report;
        const bool bCompatible = FAssetRegistry::CheckAnimationForMesh(Sequence, Mesh, &Report);
        if (!bCompatible)
        {
            UE_LOG("SetAnimation rejected: skeleton mismatch. Anim=%s Mesh=%s Reason=%s",
                Sequence->GetName().c_str(),
                Mesh->GetName().c_str(),
                Report.Reason.c_str());
        }
        return bCompatible;
    }

    return true;
}

void USkeletalMeshComponent::SetAnimation(UAnimSequenceBase* InAsset)
{
    if (!CanUseAnimation(InAsset))
    {
        return;
    }

    AnimationData.AnimToPlay = InAsset;

    if (UAnimSequence* Sequence = Cast<UAnimSequence>(InAsset))
    {
        AnimationData.AnimToPlayPath = Sequence->GetAssetPathFileName();
    }
    else if (!InAsset)
    {
        AnimationData.AnimToPlayPath = "None";
    }

    if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        SingleNode->SetAnimationAsset(InAsset);
    }
}

void USkeletalMeshComponent::SetPlayRate(float InRate)
{
    AnimationData.PlayRate = InRate;
    if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        SingleNode->SetPlayRate(InRate);
    }
}

void USkeletalMeshComponent::SetLooping(bool bInLoop)
{
    AnimationData.bLooping = bInLoop;
    if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        SingleNode->SetLooping(bInLoop);
    }
}

void USkeletalMeshComponent::SetPlaying(bool bInPlay)
{
    AnimationData.bPlaying = bInPlay;
    if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        SingleNode->SetPlaying(bInPlay);
    }
}

void USkeletalMeshComponent::SetAnimInstanceClass(UClass* InClass)
{
    if (AnimInstanceClass.Get() == InClass) return;
    AnimInstanceClass = InClass;   // TSubclassOf 가 IsA 가드로 검증 (잘못된 클래스 → nullptr).
    if (AnimationMode == EAnimationMode::AnimationCustom)
    {
        InitializeAnimation();
    }
}

void USkeletalMeshComponent::SetAnimInstance(UAnimInstance* InInstance)
{
    if (AnimInstance == InInstance) return;
    ClearAnimInstance();
    AnimInstance = InInstance;
    if (AnimInstance)
    {
        AnimInstance->SetOuter(this);
        AnimInstance->SetOwningComponent(this);
        ApplyPersistentAnimInstanceSettings(AnimInstance);
        AnimInstance->NativeInitializeAnimation();
    }
}

UAnimSingleNodeInstance* USkeletalMeshComponent::GetAnimNodeInstance(FName NodeName) const
{
    (void)NodeName;
    return Cast<UAnimSingleNodeInstance>(AnimInstance);
}

void USkeletalMeshComponent::SetRagdollEnabled(bool bEnabled)
{
    if (bEnabled) EnableRagdollPhysics();
    else DisableRagdollPhysics();
}

void USkeletalMeshComponent::EnableRagdollPhysics()
{
    ClearRagdollRecoveryState();

    if (bRagdollActive)
    {
        bRagdollEnabled = true;
        return;
    }

    DestroyRagdollConstraints();
    DestroyRagdollBodies();

    if (!CreateRagdollBodiesFromPhysicsAsset())
    {
        DestroyRagdollConstraints();
        DestroyRagdollBodies();
        bRagdollActive = false;
        bRagdollEnabled = false;
        ClearRagdollComponentSyncState();
        ClearRagdollComponentMoveState();
        ClearRagdollRecoveryState();
        UE_LOG("EnableRagdollPhysics failed: could not create ragdoll bodies");
        return;
    }

    SetAllRagdollBodiesKinematic(false);
    SetAllRagdollBodiesGravityEnabled(true);

    if (bCreateRagdollConstraints && !CreateRagdollConstraintsFromPhysicsAsset())
    {
        UE_LOG("EnableRagdollPhysics warning: no ragdoll constraints created");
    }

    CaptureRagdollComponentSyncOffset();
    CacheRagdollComponentWorldMatrix();

    bRagdollActive = true;
    bRagdollEnabled = true;

    WakeAllRagdollBodies();

    UE_LOG("Ragdoll enabled: Bodies=%zu Constraints=%zu", Bodies.size(), Constraints.size());
}

void USkeletalMeshComponent::DisableRagdollPhysics()
{
    if (!bRagdollActive && !bRagdollEnabled && Bodies.empty() && Constraints.empty())
    {
        ClearRagdollComponentSyncState();
        ClearRagdollComponentMoveState();
        ClearRagdollRecoveryState();
        return;
    }

    if (bRagdollActive)
    {
        StartRagdollRecovery();
    }

    bRagdollActive = false;
    bRagdollEnabled = false;

    DestroyRagdollConstraints();
    DestroyRagdollBodies();
    ClearRagdollComponentSyncState();
    ClearRagdollComponentMoveState();

    UE_LOG("Ragdoll disabled");
}

void USkeletalMeshComponent::ForceStopRagdollWithoutRecovery()
{
    bRagdollActive = false;
    bRagdollEnabled = false;

    DestroyRagdollConstraints();
    DestroyRagdollBodies();

    ClearRagdollComponentSyncState();
    ClearRagdollComponentMoveState();
    ClearRagdollRecoveryState();
}

void USkeletalMeshComponent::SetAllRagdollBodiesKinematic(bool bInKinematic)
{
    for (FBodyInstance* Body : Bodies)
    {
        if (!Body || !Body->IsValidBodyInstance()) continue;

        Body->SetKinematic(bInKinematic);

        UE_LOG("Ragdoll body state: Bone=%s Kinematic=%d",
            Body->BoneName.ToString().c_str(),
            bInKinematic ? 1 : 0);
    }
}

void USkeletalMeshComponent::SetAllRagdollBodiesGravityEnabled(bool bEnableGravity)
{
    for (FBodyInstance* Body : Bodies)
    {
        if (!Body || !Body->IsValidBodyInstance()) continue;

        Body->SetGravityEnabled(bEnableGravity);
    }
}

void USkeletalMeshComponent::SyncComponentToRagdollBody()
{
    if (!bHasRagdollComponentSyncOffset) return;

    FBodyInstance* SyncBody = FindRagdollComponentSyncBody();
    if (!SyncBody || !SyncBody->IsValidBodyInstance()) return;

    const FTransform BodyWorldTransform = SyncBody->GetBodyTransform();
    const FVector BodyWorldLocation = BodyWorldTransform.Location;

    FMatrix ComponentWorldNoTranslation = GetWorldMatrix();
    ComponentWorldNoTranslation.SetLocation(FVector::ZeroVector);

    const FVector WorldOffset = RagdollComponentSyncLocalOffset * ComponentWorldNoTranslation;
    const FVector NewComponentWorldLocation = BodyWorldLocation - WorldOffset;

    SetWorldLocation(NewComponentWorldLocation);
}

FBodyInstance* USkeletalMeshComponent::FindRagdollComponentSyncBody() const
{
    USkeletalMesh* Mesh = GetSkeletalMesh();
    FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;

    if (!Asset || Bodies.empty()) return nullptr;

    if (RagdollComponentSyncBoneIndex >= 0)
    {
        if (FBodyInstance* CachedBody = FindRagdollBodyByBoneIndex(RagdollComponentSyncBoneIndex))
        {
            return CachedBody;
        }
    }

    FBodyInstance* BestBody = nullptr;
    int32 BestDepth = 2147483647;

    for (FBodyInstance* Body : Bodies)
    {
        if (!Body || !Body->IsValidBodyInstance())
        {
            continue;
        }

        const int32 BoneIndex = Body->BoneIndex;
        if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Asset->Bones.size()))
        {
            continue;
        }

        int32 Depth = 0;
        int32 ParentIndex = Asset->Bones[BoneIndex].ParentIndex;
        bool bHasRagdollParent = false;

        while (ParentIndex >= 0 && ParentIndex < static_cast<int32>(Asset->Bones.size()))
        {
            ++Depth;

            if (FindRagdollBodyByBoneIndex(ParentIndex))
            {
                bHasRagdollParent = true;
                break;
            }

            ParentIndex = Asset->Bones[ParentIndex].ParentIndex;
        }

        if (!bHasRagdollParent && (!BestBody || Depth < BestDepth))
        {
            BestBody = Body;
            BestDepth = Depth;
        }
    }

    if (BestBody) return BestBody;

    for (FBodyInstance* Body : Bodies)
    {
        if (Body && Body->IsValidBodyInstance())
        {
            return Body;
        }
    }

    return nullptr;
}

void USkeletalMeshComponent::CaptureRagdollComponentSyncOffset()
{
    FBodyInstance* SyncBody = FindRagdollComponentSyncBody();
    if (!SyncBody || !SyncBody->IsValidBodyInstance())
    {
        ClearRagdollComponentSyncState();
        return;
    }

    const FMatrix ComponentWorldInv = GetWorldMatrix().GetAffineInverse();
    const FMatrix BodyComponentMatrix = SyncBody->GetBodyTransform().ToMatrix() * ComponentWorldInv;

    RagdollComponentSyncBoneIndex = SyncBody->BoneIndex;
    RagdollComponentSyncLocalOffset = BodyComponentMatrix.GetLocation();
    bHasRagdollComponentSyncOffset = true;
}

void USkeletalMeshComponent::ClearRagdollComponentSyncState()
{
    RagdollComponentSyncBoneIndex = -1;
    RagdollComponentSyncLocalOffset = FVector::ZeroVector;
    bHasRagdollComponentSyncOffset = false;
}

void USkeletalMeshComponent::CacheRagdollComponentWorldMatrix()
{
    LastRagdollComponentWorldMatrix = GetWorldMatrix();
    bHasLastRagdollComponentWorldMatrix = true;
}

void USkeletalMeshComponent::ClearRagdollComponentMoveState()
{
    LastRagdollComponentWorldMatrix = FMatrix::Identity;
    bHasLastRagdollComponentWorldMatrix = false;
}

void USkeletalMeshComponent::ApplyExternalComponentMoveToRagdollBodies()
{
    if (!bHasLastRagdollComponentWorldMatrix)
    {
        CacheRagdollComponentWorldMatrix();
        return;
    }

    const FMatrix CurrentComponentWorldMatrix = GetWorldMatrix();
    const FVector OldLocation = LastRagdollComponentWorldMatrix.GetLocation();
    const FVector NewLocation = CurrentComponentWorldMatrix.GetLocation();
    const FVector Delta = NewLocation - OldLocation;

    if (Delta.IsNearlyZero())
    {
        return;
    }

    MoveAllRagdollBodiesByComponentDelta(Delta);
}

void USkeletalMeshComponent::MoveAllRagdollBodiesByComponentDelta(const FVector& Delta)
{
    for (FBodyInstance* Body : Bodies)
    {
        if (!Body || !Body->IsValidBodyInstance())
        {
            continue;
        }

        FTransform BodyTransform = Body->GetBodyTransform();
        BodyTransform.Location = BodyTransform.Location + Delta;
        Body->SetBodyTransform(BodyTransform);
        Body->WakeUp();
    }
}

void USkeletalMeshComponent::StartRagdollRecovery()
{
    ClearRagdollRecoveryState();

    if (!bRagdollActive || Bodies.empty())
    {
        return;
    }

    SyncComponentToRagdollBody();
    SyncBonesFromRagdollBodies();
    CaptureCurrentBoneLocalPose(RagdollRecoveryStartLocalPose);

    if (RagdollRecoveryStartLocalPose.empty())
    {
        ClearRagdollRecoveryState();
        return;
    }

    bRagdollRecovering = true;
    RagdollRecoveryElapsed = 0.0f;
}

bool USkeletalMeshComponent::TickRagdollRecovery(float DeltaTime)
{
    if (!bRagdollRecovering)
    {
        return false;
    }

    if (RagdollRecoveryDuration <= 0.0f)
    {
        ClearRagdollRecoveryState();
        return false;
    }

    RagdollRecoveryElapsed += DeltaTime;

    const float Alpha = std::clamp(RagdollRecoveryElapsed / RagdollRecoveryDuration, 0.0f, 1.0f);
    const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);

    if (!EvaluateAnimInstance(DeltaTime))
    {
        ClearRagdollRecoveryState();
        return false;
    }

    if (Alpha >= 1.0f)
    {
        ClearRagdollRecoveryState();
        return true;
    }

    TArray<FTransform> TargetAnimPose;
    CaptureCurrentBoneLocalPose(TargetAnimPose);

    if (TargetAnimPose.size() != RagdollRecoveryStartLocalPose.size())
    {
        ClearRagdollRecoveryState();
        return true;
    }

    TArray<FTransform> BlendedPose;
    BlendedPose.resize(TargetAnimPose.size());

    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(TargetAnimPose.size()); ++BoneIndex)
    {
        BlendedPose[BoneIndex] = BlendBoneTransform(
            RagdollRecoveryStartLocalPose[BoneIndex],
            TargetAnimPose[BoneIndex],
            SmoothAlpha
        );
    }

    SetBoneLocalTransformsDirect(BlendedPose);
    return true;
}

void USkeletalMeshComponent::ClearRagdollRecoveryState()
{
    bRagdollRecovering = false;
    RagdollRecoveryElapsed = 0.0f;
    RagdollRecoveryStartLocalPose.clear();
}

void USkeletalMeshComponent::CaptureCurrentBoneLocalPose(TArray<FTransform>& OutPose) const
{
    OutPose.clear();

    USkeletalMesh* Mesh = GetSkeletalMesh();
    FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;

    if (!Asset || Asset->Bones.empty())
    {
        return;
    }

    const int32 BoneCount = static_cast<int32>(Asset->Bones.size());
    OutPose.resize(BoneCount);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        OutPose[BoneIndex] = GetBoneLocalTransformByIndex(BoneIndex);
    }
}

FTransform USkeletalMeshComponent::BlendBoneTransform(
    const FTransform& From,
    const FTransform& To,
    float Alpha
) const
{
    FTransform Result;
    Result.Location = FVector::Lerp(From.Location, To.Location, Alpha);
    Result.Rotation = FQuat::Slerp(From.Rotation, To.Rotation, Alpha);
    Result.Rotation.Normalize();
    Result.Scale = FVector::Lerp(From.Scale, To.Scale, Alpha);
    return Result;
}

void USkeletalMeshComponent::WakeAllRagdollBodies()
{
    for (FBodyInstance* Body : Bodies)
    {
        if (Body)
        {
            Body->WakeUp();
        }
    }
}

void USkeletalMeshComponent::AddImpulseToBone(FName BoneName, const FVector& Impulse)
{
    FBodyInstance* Body = FindRagdollBodyByBoneName(BoneName);
    if (Body)
    {
        Body->AddImpulse(Impulse);
    }
}


void USkeletalMeshComponent::LoadAnimationFromPath()
{
    AnimationData.AnimToPlay = nullptr;

    if (AnimationData.AnimToPlayPath.empty() || AnimationData.AnimToPlayPath == "None")
    {
        return;
    }

    UAnimSequence* LoadedAnimation = FAnimationManager::Get().LoadAnimation(AnimationData.AnimToPlayPath.ToString());
    if (LoadedAnimation && CanUseAnimation(LoadedAnimation))
    {
        AnimationData.AnimToPlay = LoadedAnimation;
    }
    else
    {
        AnimationData.AnimToPlay = nullptr;
    }
}

void USkeletalMeshComponent::CapturePersistentAnimInstanceSettings()
{
    if (ULuaAnimInstance* LuaAnim = Cast<ULuaAnimInstance>(AnimInstance))
    {
        if (!LuaAnim->ScriptFile.empty() && LuaAnim->ScriptFile != "None")
        {
            LuaAnimScriptFile = LuaAnim->ScriptFile;
        }
    }
}

void USkeletalMeshComponent::ApplyPersistentAnimInstanceSettings(UAnimInstance* Instance)
{
    ULuaAnimInstance* LuaAnim = Cast<ULuaAnimInstance>(Instance);
    if (!LuaAnim)
    {
        return;
    }

    if (!LuaAnimScriptFile.empty() && LuaAnimScriptFile != "None")
    {
        LuaAnim->ScriptFile = LuaAnimScriptFile;
    }
    else if (!LuaAnim->ScriptFile.empty() && LuaAnim->ScriptFile != "None")
    {
        LuaAnimScriptFile = LuaAnim->ScriptFile;
    }
}

UPhysicsAsset* USkeletalMeshComponent::GetPhysicsAssetForRagdoll()
{
    USkeletalMesh* Mesh = GetSkeletalMesh();
    if (!Mesh)
    {
        return nullptr;
    }

    if (UPhysicsAsset* PhysicsAsset = Mesh->GetPhysicsAsset())
    {
        return PhysicsAsset;
    }

    const FString& PhysicsAssetPath = Mesh->GetPhysicsAssetPath();
    if (!PhysicsAssetPath.empty() && PhysicsAssetPath != "None")
    {
        if (UPhysicsAsset* LoadedPhysicsAsset = FPhysicsAssetManager::Get().Load(PhysicsAssetPath, Mesh))
        {
            Mesh->SetPhysicsAsset(LoadedPhysicsAsset);
            return LoadedPhysicsAsset;
        }
    }

    return nullptr;
}

bool USkeletalMeshComponent::ValidatePhysicsAssetForRagdoll(UPhysicsAsset* PhysicsAsset, const FSkeletalMesh* Asset) const
{
    if (!PhysicsAsset)
    {
        UE_LOG("Ragdoll validation failed: PhysicsAsset is null");
        return false;
    }

    if (!Asset || Asset->Bones.empty())
    {
        UE_LOG("Ragdoll validation failed: SkeletalMesh asset or bones are empty");
        return false;
    }

    const TArray<UBodySetup*>& BodySetups = PhysicsAsset->GetBodySetups();
    if (BodySetups.empty())
    {
        UE_LOG("Ragdoll validation failed: PhysicsAsset has no BodySetups");
        return false;
    }

    bool bHasValidBody = false;

    for (const UBodySetup* BodySetup : BodySetups)
    {
        if (!BodySetup)
        {
            continue;
        }

        const int32 BoneIndex = FindBoneIndexByName(BodySetup->BoneName);
        if (BoneIndex < 0)
        {
            UE_LOG("Ragdoll validation warning: BodySetup bone not found. Bone=%s",
                BodySetup->BoneName.ToString().c_str());
            continue;
        }

        if (BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Disabled)
        {
            UE_LOG("Ragdoll validation warning: Body collision disabled. Bone=%s",
                BodySetup->BoneName.ToString().c_str());
            continue;
        }

        const FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();
        bool bHasShape = false;
        for (const FKSphereElem& Sphere : AggGeom.SphereElems)
        {
            bHasShape = bHasShape || ShouldCreateRagdollShape(Sphere);
        }
        for (const FKBoxElem& Box : AggGeom.BoxElems)
        {
            bHasShape = bHasShape || ShouldCreateRagdollShape(Box);
        }
        for (const FKSphylElem& Sphyl : AggGeom.SphylElems)
        {
            bHasShape = bHasShape || ShouldCreateRagdollShape(Sphyl);
        }

        if (!bHasShape)
        {
            UE_LOG("Ragdoll validation warning: BodySetup has no enabled simple shape. Bone=%s",
                BodySetup->BoneName.ToString().c_str());
            continue;
        }

        bHasValidBody = true;
    }

    if (!bHasValidBody)
    {
        UE_LOG("Ragdoll validation failed: no valid body found in PhysicsAsset");
        return false;
    }

    return true;
}

bool USkeletalMeshComponent::CreateRagdollBodiesFromPhysicsAsset()
{
    USkeletalMesh* Mesh = GetSkeletalMesh();
    FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    UPhysicsAsset* PhysicsAsset = GetPhysicsAssetForRagdoll();
    if (!ValidatePhysicsAssetForRagdoll(PhysicsAsset, Asset))
    {
        return false;
    }

    UWorld* World = GetWorld();
    IPhysicsScene* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;
    if (!PhysicsScene) return false;

    TArray<FMatrix> BoneGlobals;
    GetCurrentBoneGlobalMatrices(BoneGlobals);
    const int32 BoneCount = static_cast<int32>(Asset->Bones.size());
    if (BoneGlobals.size() != Asset->Bones.size()) return false;

    const TArray<UBodySetup*>& BodySetups = PhysicsAsset->GetBodySetups();
    Bodies.reserve(BodySetups.size());

    TArray<bool> bCreatedBoneBodies;
    bCreatedBoneBodies.resize(BoneCount, false);

    for (UBodySetup* BodySetup : BodySetups)
    {
        if (!BodySetup) continue;

        const int32 BoneIndex = FindBoneIndexByName(BodySetup->BoneName);
        if (BoneIndex < 0 || BoneIndex >= BoneCount)
        {
            UE_LOG("Ragdoll body skipped: invalid bone. Bone=%s",
                BodySetup->BoneName.ToString().c_str());
            continue;
        }

        if (bCreatedBoneBodies[BoneIndex])
        {
            UE_LOG("Ragdoll body skipped: duplicate body for bone. Bone=%s",
                BodySetup->BoneName.ToString().c_str());
            continue;
        }

        FBodyInstance* Body = new FBodyInstance();
        Body->OwnerComponent = nullptr;
        Body->OwnerSkeletalComponent = this;
        Body->BoneName = BodySetup->BoneName;
        Body->BoneIndex = BoneIndex;

        FBodyInstanceInitDesc Desc;
        if (!BuildBodyInstanceInitDescFromBodySetup(BodySetup, BoneIndex, BoneGlobals, Desc))
        {
            delete Body;
            continue;
        }

        if (PhysicsScene->CreateBodyInstance(*Body, Desc))
        {
            Bodies.push_back(Body);
            bCreatedBoneBodies[BoneIndex] = true;
        }
        else
        {
            delete Body;
        }
    }

    UE_LOG("Ragdoll bodies created: %zu", Bodies.size());
    for (FBodyInstance* Body : Bodies)
    {
        if (!Body)
        {
            continue;
        }

        UE_LOG("Ragdoll Body: Bone=%s Index=%d Valid=%d",
            Body->BoneName.ToString().c_str(),
            Body->BoneIndex,
            Body->IsValidBodyInstance() ? 1 : 0);
    }

    return !Bodies.empty();
}

bool USkeletalMeshComponent::CreateRagdollConstraintsFromPhysicsAsset()
{
    UPhysicsAsset* PhysicsAsset = GetPhysicsAssetForRagdoll();

    UWorld* World = GetWorld();
    IPhysicsScene* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;

    if (!PhysicsAsset || !PhysicsScene)
    {
        return false;
    }

    const TArray<FConstraintInstanceInitDesc>& InitDescs = PhysicsAsset->GetConstraintInitDescs();
    if (InitDescs.empty())
    {
        UE_LOG("Ragdoll constraint failed: PhysicsAsset has no ConstraintInitDescs.");
        return false;
    }

    bool bCreatedAny = false;

    for (const FConstraintInstanceInitDesc& InitDesc : InitDescs)
    {
        if (InitDesc.ParentBoneName == FName::None ||
            InitDesc.ChildBoneName == FName::None ||
            InitDesc.ParentBoneName == InitDesc.ChildBoneName)
        {
            UE_LOG("Ragdoll constraint skipped: invalid bone pair. Parent=%s Child=%s",
                InitDesc.ParentBoneName.ToString().c_str(),
                InitDesc.ChildBoneName.ToString().c_str());
            continue;
        }

        FBodyInstance* ParentBody = FindRagdollBodyByBoneName(InitDesc.ParentBoneName);
        FBodyInstance* ChildBody = FindRagdollBodyByBoneName(InitDesc.ChildBoneName);

        if (!ParentBody || !ChildBody)
        {
            UE_LOG("Ragdoll constraint skipped: missing body. Parent=%s Child=%s",
                InitDesc.ParentBoneName.ToString().c_str(),
                InitDesc.ChildBoneName.ToString().c_str());
            continue;
        }

        if (!ParentBody->IsValidBodyInstance() || !ChildBody->IsValidBodyInstance())
        {
            UE_LOG("Ragdoll constraint skipped: invalid body instance. Parent=%s Child=%s",
                InitDesc.ParentBoneName.ToString().c_str(),
                InitDesc.ChildBoneName.ToString().c_str());
            continue;
        }

        FConstraintInstance* Constraint = new FConstraintInstance();

        Constraint->ParentBody = ParentBody;
        Constraint->ChildBody = ChildBody;

        Constraint->ParentBoneName = InitDesc.ParentBoneName;
        Constraint->ChildBoneName = InitDesc.ChildBoneName;

        Constraint->ParentFrame = NormalizeConstraintFrame(InitDesc.ParentFrame);
        Constraint->ChildFrame = NormalizeConstraintFrame(InitDesc.ChildFrame);

        Constraint->TwistLimitDegrees = InitDesc.TwistLimitDegrees;
        Constraint->Swing1LimitDegrees = InitDesc.Swing1LimitDegrees;
        Constraint->Swing2LimitDegrees = InitDesc.Swing2LimitDegrees;
        Constraint->bEnableProjection = InitDesc.bEnableProjection;
        Constraint->ProjectionLinearTolerance = InitDesc.ProjectionLinearTolerance;
        Constraint->ProjectionAngularToleranceDegrees = InitDesc.ProjectionAngularToleranceDegrees;
        Constraint->bEnableCollision = ShouldRagdollConstraintEnableCollision();

        if (PhysicsScene->CreateConstraintInstance(*Constraint))
        {
            Constraints.push_back(Constraint);
            bCreatedAny = true;

            UE_LOG("Ragdoll constraint created: Parent=%s Child=%s",
                Constraint->ParentBoneName.ToString().c_str(),
                Constraint->ChildBoneName.ToString().c_str());
        }
        else
        {
            UE_LOG("Ragdoll constraint failed: Parent=%s Child=%s",
                Constraint->ParentBoneName.ToString().c_str(),
                Constraint->ChildBoneName.ToString().c_str());

            delete Constraint;
        }
    }

    UE_LOG("Ragdoll constraints created: %zu", Constraints.size());

    return bCreatedAny;
}

bool USkeletalMeshComponent::ShouldRagdollIgnoreSameOwner() const
{
    return RagdollSelfCollisionMode == ERagdollSelfCollisionMode::DisableAll;
}

bool USkeletalMeshComponent::ShouldRagdollConstraintEnableCollision() const
{
    return RagdollSelfCollisionMode == ERagdollSelfCollisionMode::EnableAll;
}

bool USkeletalMeshComponent::BuildBodyInstanceInitDescFromBodySetup( const UBodySetup* BodySetup, int32 BoneIndex, const TArray<FMatrix>& BoneGlobals, FBodyInstanceInitDesc& OutDesc) const
{
    if (!BodySetup) return false;
    if (BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Disabled) return false;

    USkeletalMesh* Mesh = GetSkeletalMesh();
    FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    if (!Asset)
    {
        return false;
    }

    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Asset->Bones.size()))
    {
        return false;
    }

    if (BoneIndex >= static_cast<int32>(BoneGlobals.size()))
    {
        return false;
    }

    OutDesc = FBodyInstanceInitDesc();

    FTransform BodyWorldTransform = FTransform::FromMatrixWithScale(BoneGlobals[BoneIndex] * GetWorldMatrix());
    const FVector BodyScale = BodyWorldTransform.Scale;

    BodyWorldTransform.Scale = FVector::OneVector;

    OutDesc.WorldTransform = BodyWorldTransform;

    OutDesc.bSimulatePhysics = true;
    OutDesc.bKinematic = BodySetup->PhysicsType == EPhysicsType::PhysType_Kinematic;

    OutDesc.CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
    OutDesc.ObjectType = ECollisionChannel::WorldDynamic;
    OutDesc.ResponseContainer.SetAllChannels(ECollisionResponse::Block);
    OutDesc.bIgnoreSameOwner = ShouldRagdollIgnoreSameOwner();

    const FBodySetupPhysicsInfo& PhysicsInfo = BodySetup->GetPhysicsInfo();
    OutDesc.Mass = BodySetup->CalculateMass(BodyScale);
    OutDesc.CenterOfMassOffset = PhysicsInfo.CenterOfMassOffset;
    OutDesc.LinearDamping = PhysicsInfo.LinearDamping;
    OutDesc.AngularDamping = PhysicsInfo.AngularDamping;
    OutDesc.bEnableGravity = PhysicsInfo.bEnableGravity;
    OutDesc.InertiaTensorScale = PhysicsInfo.InertiaTensorScale;

    const FKAggregateGeom& AggGeom = BodySetup->GetAggGeom();

    for (const FKSphereElem& Sphere : AggGeom.SphereElems)
    {
        if (!ShouldCreateRagdollShape(Sphere))
        {
            continue;
        }

        const FKSphereElem ScaledSphere = Sphere.GetFinalScaled(BodyScale, FTransform());

        FBodyShapeDesc Shape;
        Shape.ShapeType = EBodyInstanceShapeType::Sphere;
        Shape.LocalTransform = ScaledSphere.GetTransform();
        Shape.SphereRadius = std::max(ScaledSphere.Radius, 0.001f);

        OutDesc.Shapes.push_back(Shape);
    }

    for (const FKBoxElem& Box : AggGeom.BoxElems)
    {
        if (!ShouldCreateRagdollShape(Box))
        {
            continue;
        }

        const FKBoxElem ScaledBox = Box.GetFinalScaled(BodyScale, FTransform());

        FBodyShapeDesc Shape;
        Shape.ShapeType = EBodyInstanceShapeType::Box;
        Shape.LocalTransform = ScaledBox.GetTransform();
        Shape.BoxHalfExtent = FVector(
            std::max(ScaledBox.X * 0.5f, 0.001f),
            std::max(ScaledBox.Y * 0.5f, 0.001f),
            std::max(ScaledBox.Z * 0.5f, 0.001f)
        );

        OutDesc.Shapes.push_back(Shape);
    }

    for (const FKSphylElem& Sphyl : AggGeom.SphylElems)
    {
        if (!ShouldCreateRagdollShape(Sphyl))
        {
            continue;
        }

        const FKSphylElem ScaledSphyl = Sphyl.GetFinalScaled(BodyScale, FTransform());

        FBodyShapeDesc Shape;
        Shape.ShapeType = EBodyInstanceShapeType::Capsule;
        Shape.LocalTransform = ScaledSphyl.GetTransform();
        Shape.CapsuleRadius = std::max(ScaledSphyl.Radius, 0.001f);

        // FKSphylElem::Length는 실린더 구간 길이.
        // FBodyShapeDesc::CapsuleHalfHeight는 구 포함 전체 half height.
        Shape.CapsuleHalfHeight = std::max(
            ScaledSphyl.Length * 0.5f + ScaledSphyl.Radius,
            Shape.CapsuleRadius + 0.001f
        );

        OutDesc.Shapes.push_back(Shape);
    }

    if (OutDesc.Shapes.empty())
    {
        UE_LOG("Ragdoll body skipped: no valid shapes. Bone=%s",
            BodySetup->BoneName.ToString().c_str());
        return false;
    }

    return true;
}

void USkeletalMeshComponent::DestroyRagdollConstraints()
{
    UWorld* World = GetWorld();
    IPhysicsScene* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;

    for (FConstraintInstance* Constraint : Constraints)
    {
        if (!Constraint)
        {
            continue;
        }

        if (PhysicsScene)
        {
            PhysicsScene->DestroyConstraintInstance(*Constraint);
        }

        delete Constraint;
    }
    Constraints.clear();
}

void USkeletalMeshComponent::DestroyRagdollBodies()
{
    UWorld* World = GetWorld();
    IPhysicsScene* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;

    for (FBodyInstance* Body : Bodies)
    {
        if (!Body)
        {
            continue;
        }

        if (PhysicsScene)
        {
            PhysicsScene->DestroyBodyInstance(*Body);
        }

        delete Body;
    }
    Bodies.clear();
}

void USkeletalMeshComponent::SyncBonesFromRagdollBodies()
{
    USkeletalMesh* Mesh = GetSkeletalMesh();
    FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;

    if (!Asset || Asset->Bones.empty() || Bodies.empty())
    {
        return;
    }

    const int32 BoneCount = static_cast<int32>(Asset->Bones.size());

    TArray<FMatrix> OriginalGlobalMatrices;
    GetCurrentBoneGlobalMatrices(OriginalGlobalMatrices);
    if (OriginalGlobalMatrices.size() != Asset->Bones.size())
    {
        return;
    }

    TArray<FMatrix> OriginalLocalMatrices;
    OriginalLocalMatrices.resize(BoneCount, FMatrix::Identity);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const int32 ParentIndex = Asset->Bones[BoneIndex].ParentIndex;

        if (ParentIndex >= 0 && ParentIndex < BoneCount)
        {
            OriginalLocalMatrices[BoneIndex] =
                OriginalGlobalMatrices[BoneIndex] *
                OriginalGlobalMatrices[ParentIndex].GetAffineInverse();
        }
        else
        {
            OriginalLocalMatrices[BoneIndex] = OriginalGlobalMatrices[BoneIndex];
        }
    }

    const FMatrix ComponentWorldInv = GetWorldMatrix().GetAffineInverse();

    TArray<FMatrix> PhysicsGlobalMatrices;
    PhysicsGlobalMatrices.resize(BoneCount, FMatrix::Identity);

    TArray<bool> bHasPhysicsBody;
    bHasPhysicsBody.resize(BoneCount, false);

    for (FBodyInstance* Body : Bodies)
    {
        if (!Body || !Body->IsValidBodyInstance())
        {
            continue;
        }

        const int32 BoneIndex = Body->BoneIndex;
        if (BoneIndex < 0 || BoneIndex >= BoneCount)
        {
            continue;
        }

        const FMatrix BodyWorldMatrix = Body->GetBodyTransform().ToMatrix();

        // PhysX world transform -> component-space global bone transform.
        PhysicsGlobalMatrices[BoneIndex] = BodyWorldMatrix * ComponentWorldInv;
        bHasPhysicsBody[BoneIndex] = true;
    }

    TArray<FMatrix> NewGlobalMatrices;
    NewGlobalMatrices.resize(BoneCount, FMatrix::Identity);

    TArray<bool> bDrivenByPhysics;
    bDrivenByPhysics.resize(BoneCount, false);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const int32 ParentIndex = Asset->Bones[BoneIndex].ParentIndex;

        if (bHasPhysicsBody[BoneIndex])
        {
            NewGlobalMatrices[BoneIndex] = PhysicsGlobalMatrices[BoneIndex];
            bDrivenByPhysics[BoneIndex] = true;
            continue;
        }

        if (ParentIndex >= 0 && ParentIndex < BoneCount)
        {
            NewGlobalMatrices[BoneIndex] =
                OriginalLocalMatrices[BoneIndex] *
                NewGlobalMatrices[ParentIndex];

            bDrivenByPhysics[BoneIndex] = bDrivenByPhysics[ParentIndex];
        }
        else
        {
            NewGlobalMatrices[BoneIndex] = OriginalLocalMatrices[BoneIndex];
            bDrivenByPhysics[BoneIndex] = false;
        }
    }

    TArray<FTransform> LocalPose;
    LocalPose.resize(BoneCount);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const int32 ParentIndex = Asset->Bones[BoneIndex].ParentIndex;

        FMatrix LocalMatrix = NewGlobalMatrices[BoneIndex];

        if (ParentIndex >= 0 && ParentIndex < BoneCount)
        {
            LocalMatrix = NewGlobalMatrices[BoneIndex] *
                NewGlobalMatrices[ParentIndex].GetAffineInverse();
        }

        FTransform LocalTransform = FTransform::FromMatrixWithScale(LocalMatrix);
        LocalTransform.Scale = FTransform::FromMatrixWithScale(OriginalLocalMatrices[BoneIndex]).Scale;

        LocalPose[BoneIndex] = LocalTransform;
    }

    SetBoneLocalTransformsDirect(LocalPose);
}

FBodyInstance* USkeletalMeshComponent::FindRagdollBodyByBoneIndex(int32 BoneIndex) const
{
    for (FBodyInstance* Body : Bodies)
    {
        if (Body && Body->BoneIndex == BoneIndex)
        {
            return Body;
        }
    }

    return nullptr;
}

FBodyInstance* USkeletalMeshComponent::FindRagdollBodyByBoneName(FName BoneName) const
{
    for (FBodyInstance* Body : Bodies)
    {
        if (Body && Body->BoneName == BoneName)
        {
            return Body;
        }
    }

    return nullptr;
}

int32 USkeletalMeshComponent::FindBoneIndexByName(FName BoneName) const
{
    USkeletalMesh* Mesh = GetSkeletalMesh();
    FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;

    if (!Asset)
    {
        return -1;
    }

    for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Asset->Bones.size()); ++BoneIndex)
    {
        if (FName(Asset->Bones[BoneIndex].Name) == BoneName)
        {
            return BoneIndex;
        }
    }

    return -1;
}

void USkeletalMeshComponent::InitializeAnimation()
{
    if (!GetSkeletalMesh())
    {
        ClearAnimInstance();
        return;
    }
    if (AnimationMode == EAnimationMode::None)
    {
        ClearAnimInstance();
        return;
    }

    if (AnimationMode == EAnimationMode::AnimationSingleNode &&
        !AnimationData.AnimToPlay &&
        !AnimationData.AnimToPlayPath.empty() &&
        AnimationData.AnimToPlayPath != "None")
    {
        LoadAnimationFromPath();
    }

    if (AnimationMode == EAnimationMode::AnimationSingleNode && !CanUseAnimation(AnimationData.AnimToPlay))
    {
        AnimationData.AnimToPlay = nullptr;
        AnimationData.AnimToPlayPath = "None";
    }

    switch (AnimationMode)
    {
    case EAnimationMode::AnimationSingleNode:
    {
        ClearAnimInstance();

        UAnimSingleNodeInstance* Single =
            UObjectManager::Get().CreateObject<UAnimSingleNodeInstance>(this);
        AnimInstance = Single;
        Single->SetOwningComponent(this);
        Single->SetAnimationAsset(AnimationData.AnimToPlay);
        Single->SetPlayRate(AnimationData.PlayRate);
        Single->SetLooping(AnimationData.bLooping);
        Single->SetPlaying(AnimationData.bPlaying && AnimationData.AnimToPlay != nullptr);
        Single->NativeInitializeAnimation();
        break;
    }
    case EAnimationMode::AnimationCustom:
    {
        UClass* DesiredClass = AnimInstanceClass.Get();
        if (!DesiredClass)
        {
            ClearAnimInstance();
            return;
        }

        if (AnimInstance && AnimInstance->GetClass() == DesiredClass)
        {
            AnimInstance->SetOuter(this);
            AnimInstance->SetOwningComponent(this);
            ApplyPersistentAnimInstanceSettings(AnimInstance);
            AnimInstance->NativeInitializeAnimation();
            break;
        }

        ClearAnimInstance();

        UObject* Obj = FObjectFactory::Get().Create(DesiredClass->GetName(), this);
        AnimInstance = Cast<UAnimInstance>(Obj);
		if (!AnimInstance)
        {
            // 클래스가 등록 안됐거나 캐스트 실패 — 무관한 객체가 생성됐을 수 있으니 정리.
            if (Obj) UObjectManager::Get().DestroyObject(Obj);
            return;
        }
        AnimInstance->SetOwningComponent(this);
        ApplyPersistentAnimInstanceSettings(AnimInstance);

        AnimInstance->NativeInitializeAnimation();
        break;
    }
    default:
        break;
    }
}

void USkeletalMeshComponent::ClearAnimInstance()
{
    if (AnimInstance)
    {
        CapturePersistentAnimInstanceSettings();
        if (ULuaAnimInstance* LuaAnim = Cast<ULuaAnimInstance>(AnimInstance))
        {
            LuaAnim->ReleaseLuaRuntimeForShutdown();
        }
        UObjectManager::Get().DestroyObject(AnimInstance);
        AnimInstance = nullptr;
    }
}

void USkeletalMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    if (bRagdollEnabled != bRagdollActive)
    {
        SetRagdollEnabled(bRagdollEnabled);
    }

    if (bRagdollActive)
    {
        ApplyExternalComponentMoveToRagdollBodies();
        SyncComponentToRagdollBody();
        SyncBonesFromRagdollBodies();
        CacheRagdollComponentWorldMatrix();
        UMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
        return;
    }

    if (bRagdollRecovering)
    {
        if (TickRagdollRecovery(DeltaTime))
        {
            UMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
            return;
        }
    }

    if (EvaluateAnimInstance(DeltaTime))
    {
        UMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
        return;
    }

    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USkeletalMeshComponent::EndPlay()
{
    DestroyRagdollConstraints();
    DestroyRagdollBodies();
    ClearRagdollComponentSyncState();
    ClearRagdollComponentMoveState();
    ClearRagdollRecoveryState();
    Super::EndPlay();
}

// ──────────────────────────────────────────────
// Editor / 직렬화 통합
// ──────────────────────────────────────────────
void USkeletalMeshComponent::GetEditableProperties(TArray<FPropertyValue>& OutProps)
{
    Super::GetEditableProperties(OutProps);

    // AnimInstance 자체 properties (Speed 등) 도 패널에 같이 노출 — 컴포넌트가 forward.
    // 자식이 자기 카테고리(예: "Animation|Character") 로 그룹화.
    if (AnimInstance) AnimInstance->GetEditableProperties(OutProps);
}

void USkeletalMeshComponent::PostEditProperty(const char* PropertyName)
{
    Super::PostEditProperty(PropertyName);
    if (!PropertyName) return;

    if (std::strcmp(PropertyName, "AnimationMode") == 0)
    {
        InitializeAnimation();
    }
    else if (std::strcmp(PropertyName, "AnimInstanceClass") == 0)
    {
        // 클래스 슬롯이 바뀌면 Custom 모드에서 인스턴스 재생성 필요. (ours — Phase 6)
        if (AnimationMode == EAnimationMode::AnimationCustom) InitializeAnimation();
    }
    else if (std::strcmp(PropertyName, "AnimationData") == 0)
    {
        LoadAnimationFromPath();

        if (AnimInstance)
        {
            InitializeAnimation();
        }
    }
    else if (std::strcmp(PropertyName, "AnimToPlayPath") == 0)
    {
        // theirs (main): FAnimationManager 가 path 로 실제 UAnimSequence 로딩 — Phase 4 의 TODO 해소.
        // Mode 가 None 이면 SingleNode 로 자동 전환, AnimInstance 없으면 Initialize, 있으면 SingleNode setter 들 갱신.
        LoadAnimationFromPath();

        if (AnimationMode == EAnimationMode::None)
        {
            AnimationMode = EAnimationMode::AnimationSingleNode;
        }

        if (!AnimInstance)
        {
            InitializeAnimation();
        }
        else if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
        {
            if (!CanUseAnimation(AnimationData.AnimToPlay))
            {
                AnimationData.AnimToPlay = nullptr;
                AnimationData.AnimToPlayPath = "None";
            }
            SingleNode->SetAnimationAsset(AnimationData.AnimToPlay);
            SingleNode->SetPlayRate(AnimationData.PlayRate);
            SingleNode->SetLooping(AnimationData.bLooping);
            SingleNode->SetPlaying(AnimationData.bPlaying && AnimationData.AnimToPlay != nullptr);
        }
    }
    else if (std::strcmp(PropertyName, "PlayRate") == 0)
    {
        SetPlayRate(AnimationData.PlayRate);
    }
    else if (std::strcmp(PropertyName, "bLooping") == 0)
    {
        SetLooping(AnimationData.bLooping);
    }
    else if (std::strcmp(PropertyName, "bPlaying") == 0)
    {
        SetPlaying(AnimationData.bPlaying);
    }
    else if (std::strcmp(PropertyName, "LuaAnimScriptFile") == 0)
    {
        if (ULuaAnimInstance* LuaAnim = Cast<ULuaAnimInstance>(AnimInstance))
        {
            LuaAnim->ScriptFile = LuaAnimScriptFile;
            LuaAnim->ReloadScript();
        }
    }
    else if (std::strcmp(PropertyName, "bRagdollEnabled") == 0 ||
        std::strcmp(PropertyName, "Enable Ragdoll") == 0)
    {
        SetRagdollEnabled(bRagdollEnabled);
    }

    // AnimInstance 자체 properties 는 자식이 자체 PostEdit 처리. 컴포넌트는 dispatch 만.
    // 컴포넌트가 인식한 이름과 겹치지 않는 한 무해 (자식이 모르는 이름은 no-op).
    if (AnimInstance)
    {
        AnimInstance->PostEditProperty(PropertyName);
        CapturePersistentAnimInstanceSettings();
    }
}

void USkeletalMeshComponent::PostDuplicate()
{
    Super::PostDuplicate();

    // USkinnedMeshComponent::PostDuplicate() 의 SetSkeletalMesh() 경로가 이미 virtual override 를 통해
    // InitializeAnimation() 을 호출할 수 있다. 없을 때만 보강해서 PIE duplicate 의 double init 을 피한다.
    if (!AnimInstance)
    {
        InitializeAnimation();
    }
    else
    {
        ApplyPersistentAnimInstanceSettings(AnimInstance);
    }
}

void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
    if (Ar.IsSaving())
    {
        CapturePersistentAnimInstanceSettings();
    }

    Super::Serialize(Ar);

    uint8 ModeRaw = static_cast<uint8>(AnimationMode);
    Ar << ModeRaw;
    AnimationMode = static_cast<EAnimationMode>(ModeRaw);

    // AnimInstance 는 Transient 이라 Duplicate/PIE 복사에서 사라진다. Lua script path 는 컴포넌트가 별도 보관한다.
    Ar << LuaAnimScriptFile;

    // AnimToPlay 의 path 만 라운드트립. 실제 포인터 복원은 InitializeAnimation() → LoadAnimationFromPath() 가 처리.
    FString AnimToPlayPath = Ar.IsSaving() ? AnimationData.AnimToPlayPath.ToString() : FString();
    Ar << AnimToPlayPath;
    if (Ar.IsLoading())
    {
        AnimationData.AnimToPlayPath.SetPath(AnimToPlayPath);
    }
    Ar << AnimationData.PlayRate;
    Ar << AnimationData.bLooping;
    Ar << AnimationData.bPlaying;
    Ar << bRagdollEnabled;
    Ar << bCreateRagdollConstraints;
    uint8 SelfCollisionModeRaw = static_cast<uint8>(RagdollSelfCollisionMode);
    Ar << SelfCollisionModeRaw;
    RagdollSelfCollisionMode = static_cast<ERagdollSelfCollisionMode>(SelfCollisionModeRaw);
    Ar << RagdollRecoveryDuration;

}

bool USkeletalMeshComponent::EvaluateAnimInstance(float DeltaTime)
{
    if (!AnimInstance) return false;

    USkeletalMesh* Mesh = GetSkeletalMesh();
    if (!Mesh) return false;
    FSkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
    if (!Asset || Asset->Bones.empty()) return false;

    if (UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        if (!CanUseAnimation(SingleNode->GetAnimationAsset()))
        {
            SingleNode->SetAnimationAsset(nullptr);
            return false;
        }
    }

    AnimInstance->UpdateAnimation(DeltaTime);

    // Root motion 적용은 UCharacterMovementComponent 가 책임.
    // CMC::TickComponent (TG_DuringPhysics) 가 매 frame 이 AnimInstance->ConsumeRootMotion 으로
    // 누적값을 가져가 capsule 이동 / 회전에 반영한다 (sweep / floor stick 통과).
    // Mesh 는 actor transform 을 직접 만지지 않는다 — UE 본가 패턴.
    //
    // 주의: CMC 가 없는 actor 에 root motion 켠 anim 을 붙이면 누적값이 anywhere 도
    // 소비되지 않아 in-place 로 보인다. ACharacter 외 케이스에서 root motion 이 필요해지면
    // 별도 소비 경로가 추가되어야 한다.

    FPoseContext Out;
    Out.SkeletalMesh = Mesh;
    Out.Pose.resize(Asset->Bones.size());
    Out.ResetToRefPose();
    AnimInstance->EvaluatePose(Out);

    SetAnimationPose(Out.Pose, Out.MorphWeights);
    return true;
}

void USkeletalMeshComponent::AddReferencedObjects(FReferenceCollector& Collector)
{
    USkinnedMeshComponent::AddReferencedObjects(Collector);

    Collector.AddReferencedObject(AnimationData.AnimToPlay, "USkeletalMeshComponent.AnimationData.AnimToPlay");
    Collector.AddReferencedObject(AnimInstance, "USkeletalMeshComponent.AnimInstance");
}
