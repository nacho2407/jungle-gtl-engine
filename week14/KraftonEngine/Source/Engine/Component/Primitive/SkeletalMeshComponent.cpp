#include "SkeletalMeshComponent.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"

#include "Animation/AnimationManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimationRuntime.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Animation/Sequence/AnimSequenceBase.h"
#include "Animation/Instance/AnimSingleNodeInstance.h"
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

#include "Physics/RagdollInstance.h"
#include "Physics/PhysicsAsset.h"
#include "Physics/PhysicsScene.h"
#include "GameFramework/World.h"
#include "Profiling/Stats/PhysicsStats.h"
#include "Profiling/Stats/Stats.h"

#include <algorithm>
#include <cstring>

namespace
{
	bool ContainsBoneText(const FString& Text, const char* Pattern)
	{
		return Text.find(Pattern) != std::string::npos;
	}

	int32 GetRecoveryFacingBoneScore(const FString& BoneName)
	{
		if (ContainsBoneText(BoneName, "Spine2") || ContainsBoneText(BoneName, "spine2")
			|| ContainsBoneText(BoneName, "Chest") || ContainsBoneText(BoneName, "chest"))
		{
			return 4;
		}

		if (ContainsBoneText(BoneName, "Spine1") || ContainsBoneText(BoneName, "spine1")
			|| ContainsBoneText(BoneName, "Spine") || ContainsBoneText(BoneName, "spine"))
		{
			return 3;
		}

		if (ContainsBoneText(BoneName, "Pelvis") || ContainsBoneText(BoneName, "pelvis")
			|| ContainsBoneText(BoneName, "Hips") || ContainsBoneText(BoneName, "hips"))
		{
			return 2;
		}

		if (ContainsBoneText(BoneName, "Root") || ContainsBoneText(BoneName, "root"))
		{
			return 1;
		}

		return 0;
	}

	int32 FindBestRecoveryFacingBoneIndex(const FSkeletalMesh& Mesh)
	{
		int32 BestBoneIndex = -1;
		int32 BestScore = 0;

		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Mesh.Bones.size()); ++BoneIndex)
		{
			const int32 Score = GetRecoveryFacingBoneScore(Mesh.Bones[BoneIndex].Name);
			if (Score > BestScore)
			{
				BestScore = Score;
				BestBoneIndex = BoneIndex;
			}
		}

		return BestBoneIndex;
	}

	FMatrix BuildComponentSpaceBoneMatrix(
		const FSkeletalMesh& Mesh,
		const TArray<FTransform>& LocalPose,
		int32 TargetBoneIndex)
	{
		TArray<FMatrix> ComponentSpaceMatrices;
		ComponentSpaceMatrices.resize(LocalPose.size(), FMatrix::Identity);

		for (int32 BoneIndex = 0; BoneIndex <= TargetBoneIndex; ++BoneIndex)
		{
			const FMatrix LocalMatrix = LocalPose[BoneIndex].ToMatrix();
			const int32 ParentIndex = Mesh.Bones[BoneIndex].ParentIndex;

			if (ParentIndex >= 0 && ParentIndex < BoneIndex)
			{
				ComponentSpaceMatrices[BoneIndex] = LocalMatrix * ComponentSpaceMatrices[ParentIndex];
			}
			else
			{
				ComponentSpaceMatrices[BoneIndex] = LocalMatrix;
			}
		}

		return ComponentSpaceMatrices[TargetBoneIndex];
	}
}

USkeletalMeshComponent::USkeletalMeshComponent()
{
	SetCollisionPreset(ECollisionPreset::SkeletalMesh);
}

USkeletalMeshComponent::~USkeletalMeshComponent()
{
    if (bSimulatingPhysics)
    {
        SetSimulatePhysics(false);
    }

    ClearAnimInstance();
}

FPrimitiveSceneProxy* USkeletalMeshComponent::CreateSceneProxy()
{
    return new FSkeletalMeshSceneProxy(this);
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
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
        AnimInstance->NativeInitializeAnimation();
    }
}

UAnimSingleNodeInstance* USkeletalMeshComponent::GetAnimNodeInstance(FName NodeName) const
{
    (void)NodeName;
    return Cast<UAnimSingleNodeInstance>(AnimInstance);
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
        UObjectManager::Get().DestroyObject(AnimInstance);
        AnimInstance = nullptr;
    }
}

void USkeletalMeshComponent::UpdateComponentLinearVelocity(float DeltaTime)
{
	const FVector CurrentLocation = GetWorldLocation();

	if (bHasLastComponentWorldLocation && DeltaTime > 0.0f)
	{
		ComponentLinearVelocity = (CurrentLocation - LastComponentWorldLocation) / DeltaTime;
	}
	else
	{
		ComponentLinearVelocity = FVector::ZeroVector;
	}

	LastComponentWorldLocation = CurrentLocation;
	bHasLastComponentWorldLocation = true;
}

void USkeletalMeshComponent::FollowRagdollAnchor()
{
	if (!bSimulatingPhysics || !Ragdoll)
	{
		return;
	}

	FVector AnchorWorldLocation;
	if (!Ragdoll->GetAnchorWorldLocation(AnchorWorldLocation))
	{
		return;
	}

	SetWorldLocation(AnchorWorldLocation);
}

void USkeletalMeshComponent::ClearRagdollRecovery()
{
	RagdollRecoveryStartPose.clear();
	RagdollRecoveryElapsed = 0.0f;
	bRecoveringFromRagdoll = false;
}

ERagdollRecoveryFacing USkeletalMeshComponent::DetermineRagdollRecoveryFacing(const TArray<FTransform>& LocalPose) const
{
	USkeletalMesh* Mesh = GetSkeletalMesh();
	FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!MeshAsset || LocalPose.empty() || LocalPose.size() != MeshAsset->Bones.size())
	{
		return ERagdollRecoveryFacing::Unknown;
	}

	const int32 FacingBoneIndex = FindBestRecoveryFacingBoneIndex(*MeshAsset);
	if (FacingBoneIndex < 0 || FacingBoneIndex >= static_cast<int32>(LocalPose.size()))
	{
		return ERagdollRecoveryFacing::Unknown;
	}

	const FMatrix ComponentSpaceBoneMatrix =
		BuildComponentSpaceBoneMatrix(*MeshAsset, LocalPose, FacingBoneIndex);

	const FMatrix BoneWorldMatrix = ComponentSpaceBoneMatrix * GetWorldMatrix();
	FVector BodyUp = BoneWorldMatrix.TransformVector(FVector::UpVector);

	if (BodyUp.Length() <= 1.e-3f)
	{
		return ERagdollRecoveryFacing::Unknown;
	}

	BodyUp.Normalize();

	const float UpDot = BodyUp.Dot(FVector::UpVector);
	return UpDot >= 0.0f
		? ERagdollRecoveryFacing::FaceUp
		: ERagdollRecoveryFacing::FaceDown;
}

const char* USkeletalMeshComponent::GetRagdollRecoveryFacingName(ERagdollRecoveryFacing Facing) const
{
	switch (Facing)
	{
	case ERagdollRecoveryFacing::FaceUp:
		return "FaceUp";
	case ERagdollRecoveryFacing::FaceDown:
		return "FaceDown";
	default:
		return "Unknown";
	}
}

FString USkeletalMeshComponent::GetRagdollGetUpAnimationPath(ERagdollRecoveryFacing Facing) const
{
	switch (Facing)
	{
	case ERagdollRecoveryFacing::FaceUp:
		return RagdollFaceUpGetUpAnimationPath.ToString();
	case ERagdollRecoveryFacing::FaceDown:
		return RagdollFaceDownGetUpAnimationPath.ToString();
	default:
		return "None";
	}
}

void USkeletalMeshComponent::TryPlayRagdollGetUpAnimation(ERagdollRecoveryFacing Facing)
{
	const FString AnimPath = GetRagdollGetUpAnimationPath(Facing);
	if (AnimPath.empty() || AnimPath == "None")
	{
		UE_LOG(
			"Ragdoll get-up animation not set. Facing=%s",
			GetRagdollRecoveryFacingName(Facing)
		);
		return;
	}

	UAnimSequence* GetUpAnimation = FAnimationManager::Get().LoadAnimation(AnimPath);
	if (!GetUpAnimation)
	{
		UE_LOG("Ragdoll get-up animation load failed. Path=%s", AnimPath.c_str());
		return;
	}

	PlayAnimation(GetUpAnimation, false);
}

void USkeletalMeshComponent::BeginRagdollRecovery()
{
	ClearRagdollRecovery();

	if (!Ragdoll)
	{
		return;
	}

	FollowRagdollAnchor();

	if (Ragdoll->BuildLocalPoseFromBodies(this, RagdollRecoveryStartPose))
	{
		bRecoveringFromRagdoll = !RagdollRecoveryStartPose.empty();
		LastRagdollRecoveryFacing = DetermineRagdollRecoveryFacing(RagdollRecoveryStartPose);

		UE_LOG(
			"Ragdoll recovery facing: %s",
			GetRagdollRecoveryFacingName(LastRagdollRecoveryFacing)
		);
		
		TryPlayRagdollGetUpAnimation(LastRagdollRecoveryFacing);
	}
}

bool USkeletalMeshComponent::ApplyRagdollRecoveryBlend(float DeltaTime, const FPoseContext& AnimPose)
{
	if (!bRecoveringFromRagdoll || !AnimPose.IsValid())
	{
		return false;
	}

	SCOPE_STAT_CAT("Ragdoll_RecoveryBlend", "Ragdoll");

	if (RagdollRecoveryStartPose.size() != AnimPose.Pose.size())
	{
		ClearRagdollRecovery();
		return false;
	}

	RagdollRecoveryElapsed += DeltaTime;

	const float Duration = std::max(RagdollRecoveryBlendDuration, 0.0f);
	const float Alpha = Duration > 0.0f
		? std::clamp(RagdollRecoveryElapsed / Duration, 0.0f, 1.0f)
		: 1.0f;

	FPoseContext RagdollPose;
	RagdollPose.SkeletalMesh = AnimPose.SkeletalMesh;
	RagdollPose.Pose = RagdollRecoveryStartPose;

	TArray<float> BoneWeights;
	BoneWeights.assign(AnimPose.Pose.size(), Alpha);

	FPoseContext FinalPose;
	FAnimationRuntime::BlendTwoPosesPerBone(
		RagdollPose,
		AnimPose,
		BoneWeights,
		FinalPose);

	ApplyPoseToMesh(FinalPose);

	if (Alpha >= 1.0f)
	{
		ClearRagdollRecovery();
	}

	return true;
}

void USkeletalMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UpdateComponentLinearVelocity(DeltaTime);

	FPoseContext AnimPose;
	const bool bHasAnimPose = EvaluateAnimInstanceToPose(DeltaTime, AnimPose);

	if (bSimulatingPhysics && Ragdoll)
	{
		PHYSICS_STATS_RECORD_RAGDOLL_ACTIVE(Ragdoll->GetBodyCount(), Ragdoll->GetConstraintCount());

		FollowRagdollAnchor();
		
		TArray<FTransform> PhysicsLocalPose;
		if (Ragdoll->BuildLocalPoseFromBodies(this, PhysicsLocalPose))
		{
			if (bHasAnimPose)
			{
				USkeletalMesh* Mesh = GetSkeletalMesh();
				
				FPoseContext PhysicsPose;
				PhysicsPose.SkeletalMesh = Mesh;
				PhysicsPose.Pose = PhysicsLocalPose;

				TArray<float> BoneWeights;
				BoneWeights.assign(PhysicsPose.Pose.size(), PhysicsBlendWeight);
				
				FPoseContext FinalPose;
				FAnimationRuntime::BlendTwoPosesPerBone(
					AnimPose,
					PhysicsPose,
					BoneWeights,
					FinalPose);

				ApplyPoseToMesh(FinalPose);
			}
			else
			{
				SetBoneLocalTransforms(PhysicsLocalPose);
			}
			
			UMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
			return;
		}
	}
	
	if (bHasAnimPose && ApplyRagdollRecoveryBlend(DeltaTime, AnimPose))
	{
		UMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
		return;
	}

	if (bHasAnimPose)
	{
		ApplyPoseToMesh(AnimPose);
		UMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
		return;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool USkeletalMeshComponent::SyncSimulatedPhysics()
{
	if (!bSimulatingPhysics || !Ragdoll)
	{
		return false;
	}

	Ragdoll->SyncBonesFromBodies(this);
	return true;
}

void USkeletalMeshComponent::SetPhysicsBlendWeight(float InWeight)
{
	PhysicsBlendWeight = std::clamp(InWeight, 0.0f, 1.0f);
}

void USkeletalMeshComponent::StartRagdollWithVelocity(const FVector& InitialLinearVelocity)
{
	if (bSimulatingPhysics)
	{
		return;
	}

	ComponentLinearVelocity = InitialLinearVelocity;
	SetSimulatePhysics(true);
}

void USkeletalMeshComponent::SetSimulatePhysics(bool bEnable)
{
	if (bEnable == bSimulatingPhysics)
	{
		return;
	}

	UWorld* World = GetWorld();
	FPhysicsScene* Scene = World ? World->GetPhysicsScene() : nullptr;
	if (!Scene)
	{
		UE_LOG("Ragdoll toggle skipped: PhysicsScene not available.");
		if (bEnable)
		{
			PHYSICS_STATS_RECORD_RAGDOLL_START(false);
		}
		return; // 물리 씬 없음(예: 에디터 프리뷰) → 래그돌 불가
	}

	if (bEnable)
	{
		ClearRagdollRecovery();
		
		UPhysicsAsset* Asset = GetPhysicsAsset(); // 3-B: 오버라이드 ?? 메시 기본
		if (!Asset)
		{
			USkeletalMesh* Mesh = GetSkeletalMesh();
			UE_LOG("Ragdoll enable skipped: PhysicsAsset not found. Mesh=%s",
				Mesh ? Mesh->GetAssetPathFileName().c_str() : "None");
			PHYSICS_STATS_RECORD_RAGDOLL_START(false);
			return; // PhysicsAsset 링크 없음
		}

		if (!Ragdoll)
		{
			Ragdoll = std::make_unique<FRagdollInstance>();
		}
		Ragdoll->Initialize(Asset, this, Scene, ComponentLinearVelocity);
		bSimulatingPhysics = Ragdoll->IsActive();
		PHYSICS_STATS_RECORD_RAGDOLL_START(bSimulatingPhysics);
		if (!bSimulatingPhysics)
		{
			UE_LOG("Ragdoll enable failed: instance did not initialize. Mesh=%s PhysicsAsset=%s",
				GetSkeletalMesh() ? GetSkeletalMesh()->GetAssetPathFileName().c_str() : "None",
				Asset->GetSourcePath().c_str());
		}
	}
	else
	{
		if (Ragdoll)
		{
			BeginRagdollRecovery();
			Ragdoll->Release(Scene);
		}
		bSimulatingPhysics = false;
	}
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

    // AnimInstance 자체 properties 는 자식이 자체 PostEdit 처리. 컴포넌트는 dispatch 만.
    // 컴포넌트가 인식한 이름과 겹치지 않는 한 무해 (자식이 모르는 이름은 no-op).
    if (AnimInstance) AnimInstance->PostEditProperty(PropertyName);
}

void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);

    uint8 ModeRaw = static_cast<uint8>(AnimationMode);
    Ar << ModeRaw;
    AnimationMode = static_cast<EAnimationMode>(ModeRaw);

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

}

// 애니메이션 업데이트라고, 결과 포즈를 OutPose에 담음.
bool USkeletalMeshComponent::EvaluateAnimInstanceToPose(float DeltaTime, FPoseContext& OutPose)
{
	OutPose.SkeletalMesh = nullptr;
	OutPose.Pose.clear();
	OutPose.MorphWeights.clear();

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

	OutPose.SkeletalMesh = Mesh;
	OutPose.Pose.resize(Asset->Bones.size());
	OutPose.ResetToRefPose();

	AnimInstance->EvaluatePose(OutPose);
	return true;
}

// 이미 계산된 pose를 실제 mesh에 적용.
void USkeletalMeshComponent::ApplyPoseToMesh(const FPoseContext& Pose)
{
	if (!Pose.IsValid())
	{
		return;
	}

	SetAnimationPose(Pose.Pose, Pose.MorphWeights);
}

bool USkeletalMeshComponent::EvaluateAnimInstance(float DeltaTime)
{
	FPoseContext AnimPose;
	if (!EvaluateAnimInstanceToPose(DeltaTime, AnimPose))
	{
		return false;
	}

	ApplyPoseToMesh(AnimPose);
	return true;
}
