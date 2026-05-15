#include "SkeletalMeshComponent.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Object/ObjectFactory.h"
#include "Object/UClass.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/PoseContext.h"

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

USkeletalMeshComponent::~USkeletalMeshComponent()
{
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

// ──────────────────────────────────────────────
// Animation API
// ──────────────────────────────────────────────
void USkeletalMeshComponent::SetAnimationMode(EAnimationMode InMode)
{
	if (AnimationMode == InMode) return;
	AnimationMode = InMode;
	InitializeAnimation();
}

void USkeletalMeshComponent::SetAnimation(UAnimSequenceBase* InAsset)
{
	AnimationData.AnimToPlay = InAsset;
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
	if (AnimInstanceClass == InClass) return;
	AnimInstanceClass = InClass;
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

void USkeletalMeshComponent::InitializeAnimation()
{
	// AnimInstance 는 항상 재생성. (이미 있는 경우라도 SkeletalMesh/Mode/Class 가 바뀌었을 수 있음)
	ClearAnimInstance();

	if (!GetSkeletalMesh()) return;
	if (AnimationMode == EAnimationMode::None) return;

	switch (AnimationMode)
	{
	case EAnimationMode::AnimationSingleNode:
	{
		UAnimSingleNodeInstance* Single =
			UObjectManager::Get().CreateObject<UAnimSingleNodeInstance>(this);
		AnimInstance = Single;
		Single->SetOwningComponent(this);
		Single->SetAnimationAsset(AnimationData.AnimToPlay);
		Single->SetPlayRate(AnimationData.PlayRate);
		Single->SetLooping(AnimationData.bLooping);
		Single->SetPlaying(AnimationData.bPlaying);
		Single->NativeInitializeAnimation();
		break;
	}
	case EAnimationMode::AnimationCustom:
	{
		if (!AnimInstanceClass) return;
		UObject* Obj = FObjectFactory::Get().Create(AnimInstanceClass->GetName(), this);
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

void USkeletalMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	// Animation 평가는 Super 의 UpdateCPUSkinning 보다 먼저 — Super 가 끝에 1회만 스키닝하므로
	// 우리가 미리 BoneEditLocalMatrices 를 채워두면 새 포즈로 1회 스키닝된다.
	EvaluateAnimInstance(DeltaTime);

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USkeletalMeshComponent::EvaluateAnimInstance(float DeltaTime)
{
	if (!AnimInstance) return;

	USkeletalMesh* Mesh = GetSkeletalMesh();
	if (!Mesh) return;
	FSkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
	if (!Asset || Asset->Bones.empty()) return;

	AnimInstance->UpdateAnimation(DeltaTime);

	FPoseContext Out;
	Out.SkeletalMesh = Mesh;
	Out.Pose.resize(Asset->Bones.size());
	Out.ResetToRefPose();
	AnimInstance->EvaluatePose(Out);

	// 본 단위 setter 는 호출마다 UpdateCPUSkinning 을 돌리므로 직접 행렬 배열에 쓴다.
	// EnsureBoneEditPose 는 size 보장 + 첫 진입 시 LocalMatrix 로 시드.
	EnsureBoneEditPose();
	const size_t N = std::min<size_t>(Out.Pose.size(), BoneEditLocalMatrices.size());
	for (size_t i = 0; i < N; ++i)
	{
		BoneEditLocalMatrices[i] = Out.Pose[i].ToMatrix();
	}
	bUseBoneEditPose = true;
}
