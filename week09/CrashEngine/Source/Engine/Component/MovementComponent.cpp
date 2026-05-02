// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "Component/MovementComponent.h"

#include "Component/SceneComponent.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

// Base movement logic only; concrete movement types should be added instead.
IMPLEMENT_ABSTRACT_COMPONENT_CLASS(UMovementComponent, UActorComponent)

// ============================================================
// 경로 헬퍼 (파일 스코프)
// ============================================================

// Root → Target 의 경로를 outIndices 에 쌓는다. 찾으면 true.
static bool FindPathHelper(USceneComponent* Current, USceneComponent* Target, TArray<int32>& OutIndices)
{
    if (Current == Target)
        return true;
    const TArray<USceneComponent*>& Children = Current->GetChildren();
    for (int32 i = 0; i < static_cast<int32>(Children.size()); ++i)
    {
        OutIndices.push_back(i);
        if (FindPathHelper(Children[i], Target, OutIndices))
            return true;
        OutIndices.pop_back();
    }
    return false;
}

// SceneComponent 트리에서 Target 의 계층 경로를 계산한다.
//   Root → "Root",  Root->child[1]->child[0] → "1/0",  미발견 → ""
static FString ComputeComponentPath(USceneComponent* Root, USceneComponent* Target)
{
    if (!Root || !Target)
        return "";
    if (Root == Target)
        return "Root";

    TArray<int32> Indices;
    if (!FindPathHelper(Root, Target, Indices))
        return "";

    FString Path;
    for (int32 i = 0; i < static_cast<int32>(Indices.size()); ++i)
    {
        if (i > 0)
            Path += "/";
        Path += std::to_string(Indices[i]);
    }
    return Path;
}

// 계층 경로로 SceneComponent 포인터를 복원한다.
//   "Root" → Root,  "1/0" → Root->child[1]->child[0],  "" / 실패 → nullptr
static USceneComponent* ResolveComponentFromPath(USceneComponent* Root, const FString& Path)
{
    if (!Root || Path.empty())
        return nullptr;
    if (Path == "Root")
        return Root;

    USceneComponent* Current = Root;
    size_t Start = 0;
    while (Start < Path.size())
    {
        const size_t End = Path.find('/', Start);
        const size_t Len = (End == FString::npos) ? (Path.size() - Start) : (End - Start);

        const int32 Idx = std::stoi(Path.substr(Start, Len));
        const TArray<USceneComponent*>& Children = Current->GetChildren();
        if (Idx < 0 || Idx >= static_cast<int32>(Children.size()))
            return nullptr;
        Current = Children[Idx];

        Start = (End == FString::npos) ? Path.size() : (End + 1);
    }
    return Current;
}

// ============================================================

void UMovementComponent::BeginPlay()
{
    UActorComponent::BeginPlay();
    TryAutoRegisterUpdatedComponent();
}

void UMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Auto Register Updated", EPropertyType::Bool, &bAutoRegisterUpdatedComponent });
    OutProps.push_back({ "Updated Component", EPropertyType::ComponentRef, &UpdatedComponentPath });
}

void UMovementComponent::PostEditProperty(const char* PropertyName)
{
    UActorComponent::PostEditProperty(PropertyName);
    if (strcmp(PropertyName, "Updated Component") == 0)
    {
        UpdatedComponent = nullptr;
        ResolveUpdatedComponentByPath();
    }
}

void UMovementComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);
    Ar << bAutoRegisterUpdatedComponent;
    Ar << UpdatedComponentPath;
}

void UMovementComponent::PostDuplicate()
{
    UActorComponent::PostDuplicate();
    // Serialize 왕복 후 UpdatedComponent 포인터는 유효하지 않으므로 경로로 재해결한다.
    // PostDuplicate 시점에는 Owner가 아직 없지만 Outer = 새 액터이므로 GetTypedOuter로 접근.
    UpdatedComponent = nullptr;
    ResolveUpdatedComponentByPath();
}

void UMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
    UpdatedComponent = NewUpdatedComponent;

    // 포인터 → 경로 역산 (액터가 있을 때만 가능)
    AActor* ActorOwner = Owner ? Owner : GetTypedOuter<AActor>();
    if (ActorOwner && ActorOwner->GetRootComponent())
    {
        UpdatedComponentPath = ComputeComponentPath(ActorOwner->GetRootComponent(), NewUpdatedComponent);
    }
    else
    {
        UpdatedComponentPath.clear();
    }
}

void UMovementComponent::ResolveUpdatedComponentByPath()
{
    if (UpdatedComponentPath.empty())
        return;

    AActor* ActorOwner = Owner ? Owner : GetTypedOuter<AActor>();
    if (!ActorOwner)
        return;

    UpdatedComponent = ResolveComponentFromPath(ActorOwner->GetRootComponent(), UpdatedComponentPath);
}

void UMovementComponent::TryAutoRegisterUpdatedComponent()
{
    if (UpdatedComponent != nullptr)
        return; // 이미 해결됨

    // 경로가 명시된 경우 경로 우선
    if (!UpdatedComponentPath.empty())
    {
        ResolveUpdatedComponentByPath();
        return;
    }

    // 경로 미지정 + Auto 플래그 → Root 컴포넌트 사용
    if (bAutoRegisterUpdatedComponent)
    {
        AActor* OwnerActor = GetOwner();
        if (OwnerActor)
        {
            // SetUpdatedComponent 대신 직접 할당 — 경로를 덮어쓰지 않기 위해
            UpdatedComponent = OwnerActor->GetRootComponent();
        }
    }
}
