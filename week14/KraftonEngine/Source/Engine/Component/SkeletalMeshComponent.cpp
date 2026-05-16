#include "SkeletalMeshComponent.h"

#include "Animation/AnimationManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/PoseContext.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Object/UClass.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryArchive.h"

#include <algorithm>
#include <cstring>

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

void USkeletalMeshComponent::SetAnimation(UAnimSequenceBase* InAsset)
{
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

    UAnimSequence* LoadedAnimation = FAnimationManager::Get().LoadAnimation(AnimationData.AnimToPlayPath);
    AnimationData.AnimToPlay = LoadedAnimation;
}

void USkeletalMeshComponent::InitializeAnimation()
{
    // AnimInstance 는 항상 재생성. (이미 있는 경우라도 SkeletalMesh/Mode/Class 가 바뀌었을 수 있음)
    ClearAnimInstance();

    if (!GetSkeletalMesh()) return;
    if (AnimationMode == EAnimationMode::None) return;

    if (AnimationMode == EAnimationMode::AnimationSingleNode &&
        !AnimationData.AnimToPlay &&
        !AnimationData.AnimToPlayPath.empty() &&
        AnimationData.AnimToPlayPath != "None")
    {
        LoadAnimationFromPath();
    }

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
        Single->SetPlaying(AnimationData.bPlaying && AnimationData.AnimToPlay != nullptr);
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

        // Editor-set 데이터 (Serialize 라운드트립으로 들어온 ScriptFile 등) 가 buffer 에 있으면
        // 새 AnimInstance 에 주입 — NativeInitializeAnimation 보다 먼저.
        if (!AnimInstanceData.empty())
        {
            FMemoryArchive Reader(AnimInstanceData, /*bIsSaving*/false);
            AnimInstance->Serialize(Reader);
        }

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
    if (EvaluateAnimInstance(DeltaTime))
    {
        UMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
        return;
    }

    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// ──────────────────────────────────────────────
// Editor / 직렬화 통합
// ──────────────────────────────────────────────
void USkeletalMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    Super::GetEditableProperties(OutProps);

    FPropertyDescriptor ModeProp;
    ModeProp.Name       = "Animation Mode";
    ModeProp.Type       = EPropertyType::Enum;
    ModeProp.Category   = "Animation";
    ModeProp.ValuePtr   = &AnimationMode;
    ModeProp.EnumNames  = GAnimationModeNames;
    ModeProp.EnumCount  = GAnimationModeCount;
    ModeProp.EnumSize   = sizeof(EAnimationMode);
    OutProps.push_back(ModeProp);

    FPropertyDescriptor ClassProp;
    ClassProp.Name      = "Anim Instance Class";
    ClassProp.Type      = EPropertyType::ClassRef;
    ClassProp.Category  = "Animation";
    ClassProp.ValuePtr  = &AnimInstanceClass;                          // TSubclassOf 의 UClass* 멤버 위치
    ClassProp.ClassBase = TSubclassOf<UAnimInstance>::StaticClass();   // 자식 enumerate 베이스
    OutProps.push_back(ClassProp);

    FPropertyDescriptor AnimProp;
    AnimProp.Name          = "Anim To Play";
    AnimProp.Type          = EPropertyType::ObjectRef;
    AnimProp.Category      = "Animation";
    AnimProp.ValuePtr      = &AnimationData.AnimToPlayPath;
    AnimProp.AssetTypeName = "UAnimSequence";
    OutProps.push_back(AnimProp);

    FPropertyDescriptor PlayRateProp;
    PlayRateProp.Name     = "Play Rate";
    PlayRateProp.Type     = EPropertyType::Float;
    PlayRateProp.Category = "Animation";
    PlayRateProp.ValuePtr = &AnimationData.PlayRate;
    PlayRateProp.Min      = -4.0f;
    PlayRateProp.Max      = 4.0f;
    PlayRateProp.Speed    = 0.05f;
    OutProps.push_back(PlayRateProp);

    FPropertyDescriptor LoopProp;
    LoopProp.Name     = "Looping";
    LoopProp.Type     = EPropertyType::Bool;
    LoopProp.Category = "Animation";
    LoopProp.ValuePtr = &AnimationData.bLooping;
    OutProps.push_back(LoopProp);

    FPropertyDescriptor PlayingProp;
    PlayingProp.Name     = "Playing";
    PlayingProp.Type     = EPropertyType::Bool;
    PlayingProp.Category = "Animation";
    PlayingProp.ValuePtr = &AnimationData.bPlaying;
    OutProps.push_back(PlayingProp);

    // AnimInstance 자체 properties (Speed 등) 도 패널에 같이 노출 — 컴포넌트가 forward.
    // 자식이 자기 카테고리(예: "Animation|Character") 로 그룹화.
    if (AnimInstance) AnimInstance->GetEditableProperties(OutProps);
}

void USkeletalMeshComponent::PostEditProperty(const char* PropertyName)
{
    Super::PostEditProperty(PropertyName);
    if (!PropertyName) return;

    if (std::strcmp(PropertyName, "Animation Mode") == 0)
    {
        InitializeAnimation();
    }
    else if (std::strcmp(PropertyName, "Anim Instance Class") == 0)
    {
        // 클래스 슬롯이 바뀌면 Custom 모드에서 인스턴스 재생성 필요. (ours — Phase 6)
        if (AnimationMode == EAnimationMode::AnimationCustom) InitializeAnimation();
    }
    else if (std::strcmp(PropertyName, "Anim To Play") == 0)
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
            SingleNode->SetAnimationAsset(AnimationData.AnimToPlay);
            SingleNode->SetPlayRate(AnimationData.PlayRate);
            SingleNode->SetLooping(AnimationData.bLooping);
            SingleNode->SetPlaying(AnimationData.bPlaying && AnimationData.AnimToPlay != nullptr);
        }
    }
    else if (std::strcmp(PropertyName, "Play Rate") == 0)
    {
        SetPlayRate(AnimationData.PlayRate);
    }
    else if (std::strcmp(PropertyName, "Looping") == 0)
    {
        SetLooping(AnimationData.bLooping);
    }
    else if (std::strcmp(PropertyName, "Playing") == 0)
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
    Ar << AnimationData.AnimToPlayPath;
    Ar << AnimationData.PlayRate;
    Ar << AnimationData.bLooping;
    Ar << AnimationData.bPlaying;

    // AnimInstanceClass — 클래스 이름 라운드트립. 복원 시 TSubclassOf 의 IsA 가드로 잘못된 클래스 자동 nullptr.
    if (Ar.IsLoading())
    {
        FString ClassName;
        Ar << ClassName;
        AnimInstanceClass = (ClassName.empty() || ClassName == "None")
            ? nullptr
            : UClass::FindByName(ClassName.c_str());
    }
    else
    {
        FString ClassName = AnimInstanceClass.Get() ? FString(AnimInstanceClass.Get()->GetName()) : FString("None");
        Ar << ClassName;
    }

    // AnimInstance 의 Editor-set 데이터 (ScriptFile 등) opaque buffer 라운드트립.
    // Save 시 live 인스턴스에서 매번 재추출 — 옵션 (a): Editor 작업 직후에는 갱신 안 하고 Save 시점 1회.
    // Load 시 buffer 만 채우고, 실제 적용은 InitializeAnimation 안에서 (AnimInstance 생성 직후).
    if (Ar.IsSaving() && AnimInstance)
    {
        FMemoryArchive Writer(/*bIsSaving*/true);
        AnimInstance->Serialize(Writer);
        AnimInstanceData = Writer.GetBuffer();
    }
    Ar << AnimInstanceData;
}

bool USkeletalMeshComponent::EvaluateAnimInstance(float DeltaTime)
{
    if (!AnimInstance) return false;

    USkeletalMesh* Mesh = GetSkeletalMesh();
    if (!Mesh) return false;
    FSkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
    if (!Asset || Asset->Bones.empty()) return false;

    AnimInstance->UpdateAnimation(DeltaTime);

    FPoseContext Out;
    Out.SkeletalMesh = Mesh;
    Out.Pose.resize(Asset->Bones.size());
    Out.ResetToRefPose();
    AnimInstance->EvaluatePose(Out);

    SetBoneLocalTransforms(Out.Pose);
    return true;
}
