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


#include <algorithm>
#include <cstring>

#include "Object/GarbageCollection.h"

namespace
{
    float EstimateRagdollRadiusFromChildren(
        const FSkeletalMesh& Asset,
        int32 BoneIndex,
        const TArray<FMatrix>& Globals
    )
    {
        float MaxChildDist = 0.0f;
        const FVector BonePos = Globals[BoneIndex].GetLocation();

        for (int32 ChildIndex = 0; ChildIndex < static_cast<int32>(Asset.Bones.size()); ++ChildIndex)
        {
            if (Asset.Bones[ChildIndex].ParentIndex != BoneIndex)
            {
                continue;
            }

            const float Dist = FVector::Distance(BonePos, Globals[ChildIndex].GetLocation());
            MaxChildDist = std::max(MaxChildDist, Dist);
        }

        if (MaxChildDist <= 0.0f)
        {
            return 8.0f;
        }

        return std::max(4.0f, std::min(MaxChildDist * 0.20f, 18.0f));
    }
}

USkeletalMeshComponent::~USkeletalMeshComponent()
{
    DestroyRagdollBodies();
    ClearAnimInstance();
}

FPrimitiveSceneProxy* USkeletalMeshComponent::CreateSceneProxy()
{
    return new FSkeletalMeshSceneProxy(this);
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
    if (bRagdollActive)
    {
        SetRagdollEnabled(false);
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
    if (bRagdollActive == bEnabled)
    {
        bRagdollEnabled = bEnabled;
        return;
    }

    if (!bEnabled)
    {
        DestroyRagdollBodies();
        bRagdollActive = false;
        bRagdollEnabled = false;
        return;
    }

    DestroyRagdollBodies();

    if (!CreateRagdollBodiesFromPhysicsAsset())
    {
        DestroyRagdollBodies();
        bRagdollActive = false;
        bRagdollEnabled = false;
        return;
    }

    CreateRagdollConstraintsFromPhysicsAsset();

    bRagdollActive = true;
    bRagdollEnabled = true;

    WakeAllRagdollBodies();
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

UPhysicsAsset* USkeletalMeshComponent::GetPhysicsAssetForRagdoll() const
{
    USkeletalMesh* Mesh = GetSkeletalMesh();
    if (!Mesh) return nullptr;

    return Mesh->GetPhysicsAsset();
}

bool USkeletalMeshComponent::CreateRagdollBodiesFromPhysicsAsset()
{
    USkeletalMesh* Mesh = GetSkeletalMesh();
    FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    UPhysicsAsset* PhysicsAsset = GetPhysicsAssetForRagdoll();
    if (!Asset || !PhysicsAsset) return false;

    UWorld* World = GetWorld();
    IPhysicsScene* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;
    if (!PhysicsScene) return false;

    TArray<FMatrix> BoneGlobals;
    GetCurrentBoneGlobalMatrices(BoneGlobals);
    if (BoneGlobals.size() != Asset->Bones.size()) return false;

    const TArray<UBodySetup*>& BodySetups = PhysicsAsset->GetBodySetups();
    Bodies.reserve(BodySetups.size());

    for (UBodySetup* BodySetup : BodySetups)
    {
        if (!BodySetup) continue;

        const int32 BoneIndex = FindBoneIndexByName(BodySetup->BoneName);
        if (BoneIndex < 0) continue;

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
        }
        else
        {
            delete Body;
        }
    }

    return !Bodies.empty();
}

bool USkeletalMeshComponent::CreateRagdollConstraintsFromPhysicsAsset()
{
    UPhysicsAsset* PhysicsAsset = GetPhysicsAssetForRagdoll();

    UWorld* World = GetWorld();
    IPhysicsScene* PhysicsScene = World ? World->GetPhysicsScene() : nullptr;

    if (!PhysicsAsset || !PhysicsScene) return false;

    bool bCreatedAny = false;

    for (const FConstraintInstanceInitDesc& InitDesc : PhysicsAsset->GetConstraintInitDescs())
    {
        FBodyInstance* ParentBody = FindRagdollBodyByBoneName(InitDesc.ParentBoneName);
        FBodyInstance* ChildBody = FindRagdollBodyByBoneName(InitDesc.ChildBoneName);

        if (!ParentBody || !ChildBody) continue;

        FConstraintInstance* Constraint = new FConstraintInstance();

        Constraint->ParentBody = ParentBody;
        Constraint->ChildBody = ChildBody;

        Constraint->ParentBoneName = InitDesc.ParentBoneName;
        Constraint->ChildBoneName = InitDesc.ChildBoneName;

        Constraint->ParentFrame = InitDesc.ParentFrame;
        Constraint->ChildFrame = InitDesc.ChildFrame;

        Constraint->TwistLimitDegrees = InitDesc.TwistLimitDegrees;
        Constraint->Swing1LimitDegrees = InitDesc.Swing1LimitDegrees;
        Constraint->Swing2LimitDegrees = InitDesc.Swing2LimitDegrees;

        Constraint->bEnableCollision = InitDesc.bEnableCollision;

        if (PhysicsScene->CreateConstraintInstance(*Constraint))
        {
            Constraints.push_back(Constraint);
            bCreatedAny = true;
        }
        else
        {
            delete Constraint;
        }
    }

    return bCreatedAny;
}

bool USkeletalMeshComponent::BuildBodyInstanceInitDescFromBodySetup( const UBodySetup* BodySetup, int32 BoneIndex, const TArray<FMatrix>& BoneGlobals, FBodyInstanceInitDesc& OutDesc) const
{
    if (!BodySetup) return false;
    if (BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Disabled) return false;

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
        const FKSphereElem ScaledSphere = Sphere.GetFinalScaled(BodyScale, FTransform());

        FBodyShapeDesc Shape;
        Shape.ShapeType = EBodyInstanceShapeType::Sphere;
        Shape.LocalTransform = ScaledSphere.GetTransform();
        Shape.SphereRadius = std::max(ScaledSphere.Radius, 0.001f);

        OutDesc.Shapes.push_back(Shape);
    }

    for (const FKBoxElem& Box : AggGeom.BoxElems)
    {
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

    return !OutDesc.Shapes.empty();
}

void USkeletalMeshComponent::DestroyRagdollBodies()
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
    OriginalGlobalMatrices.resize(BoneCount, FMatrix::Identity);

    GetCurrentBoneGlobalMatrices(OriginalGlobalMatrices);
    if (OriginalGlobalMatrices.size() != Asset->Bones.size())
    {
        OriginalGlobalMatrices.resize(BoneCount, FMatrix::Identity);
    }

    const FMatrix ComponentWorldInv = GetWorldMatrix().GetAffineInverse();

    TArray<FMatrix> BodyGlobalMatrices;
    BodyGlobalMatrices.resize(BoneCount, FMatrix::Identity);

    TArray<bool> bHasPhysicsBody;
    bHasPhysicsBody.resize(BoneCount, false);

    TArray<int32> PhysicsBodyBoneIndices;

    for (FBodyInstance* Body : Bodies)
    {
        if (!Body || !Body->IsValidBodyInstance())
        {
            continue;
        }

        if (Body->BoneIndex < 0 || Body->BoneIndex >= BoneCount)
        {
            continue;
        }

        const FMatrix BodyWorld = Body->GetBodyTransform().ToMatrix();

        // world → component local global
        BodyGlobalMatrices[Body->BoneIndex] = BodyWorld * ComponentWorldInv;
        bHasPhysicsBody[Body->BoneIndex] = true;
        PhysicsBodyBoneIndices.push_back(Body->BoneIndex);
    }

    TArray<FMatrix> OriginalLocalMatrices;
    OriginalLocalMatrices.resize(BoneCount, FMatrix::Identity);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const int32 ParentIndex = Asset->Bones[BoneIndex].ParentIndex;

        OriginalLocalMatrices[BoneIndex] = OriginalGlobalMatrices[BoneIndex];

        if (ParentIndex >= 0 && ParentIndex < BoneCount)
        {
            OriginalLocalMatrices[BoneIndex] =
                OriginalGlobalMatrices[BoneIndex] *
                OriginalGlobalMatrices[ParentIndex].GetAffineInverse();
        }
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
            NewGlobalMatrices[BoneIndex] = BodyGlobalMatrices[BoneIndex];
            bDrivenByPhysics[BoneIndex] = true;
        }
        else if (ParentIndex >= 0 && ParentIndex < BoneCount)
        {
            if (bDrivenByPhysics[ParentIndex])
            {
                NewGlobalMatrices[BoneIndex] =
                    OriginalLocalMatrices[BoneIndex] *
                    NewGlobalMatrices[ParentIndex];
                bDrivenByPhysics[BoneIndex] = true;
            }
            else
            {
                NewGlobalMatrices[BoneIndex] =
                    OriginalLocalMatrices[BoneIndex] *
                    NewGlobalMatrices[ParentIndex];
            }
        }
        else
        {
            NewGlobalMatrices[BoneIndex] = OriginalLocalMatrices[BoneIndex];
        }

        if (!bDrivenByPhysics[BoneIndex] && !PhysicsBodyBoneIndices.empty())
        {
            int32 NearestBodyBoneIndex = -1;
            float NearestDistSq = 3.402823466e+38F;

            const FVector BoneLocation = OriginalGlobalMatrices[BoneIndex].GetLocation();
            for (int32 BodyBoneIndex : PhysicsBodyBoneIndices)
            {
                if (BodyBoneIndex < 0 || BodyBoneIndex >= BoneCount)
                {
                    continue;
                }

                const FVector BodyLocation = OriginalGlobalMatrices[BodyBoneIndex].GetLocation();
                const FVector Delta = BoneLocation - BodyLocation;
                const float DistSq = Delta.X * Delta.X + Delta.Y * Delta.Y + Delta.Z * Delta.Z;

                if (DistSq < NearestDistSq)
                {
                    NearestDistSq = DistSq;
                    NearestBodyBoneIndex = BodyBoneIndex;
                }
            }

            if (NearestBodyBoneIndex >= 0)
            {
                const FMatrix OffsetFromBody =
                    OriginalGlobalMatrices[BoneIndex] *
                    OriginalGlobalMatrices[NearestBodyBoneIndex].GetAffineInverse();

                NewGlobalMatrices[BoneIndex] =
                    OffsetFromBody *
                    BodyGlobalMatrices[NearestBodyBoneIndex];
                bDrivenByPhysics[BoneIndex] = true;
            }
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

        LocalPose[BoneIndex] = FTransform::FromMatrixWithScale(LocalMatrix);
        LocalPose[BoneIndex].Scale = FTransform::FromMatrixWithScale(OriginalLocalMatrices[BoneIndex]).Scale;
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

float USkeletalMeshComponent::EstimateRagdollRadiusFromChildren(
    const FSkeletalMesh& Asset,
    int32 BoneIndex,
    const TArray<FMatrix>& Globals
)
{
    float MaxChildDist = 0.0f;
    const FVector BonePos = Globals[BoneIndex].GetLocation();

    for (int32 ChildIndex = 0; ChildIndex < static_cast<int32>(Asset.Bones.size()); ++ChildIndex)
    {
        if (Asset.Bones[ChildIndex].ParentIndex != BoneIndex)
        {
            continue;
        }

        const float Dist = FVector::Distance(BonePos, Globals[ChildIndex].GetLocation());
        MaxChildDist = std::max(MaxChildDist, Dist);
    }

    if (MaxChildDist <= 0.0f)
    {
        return 8.0f;
    }

    return std::max(4.0f, std::min(MaxChildDist * 0.20f, 18.0f));
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
        SyncBonesFromRagdollBodies();
        UMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
        return;
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
    DestroyRagdollBodies();
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
