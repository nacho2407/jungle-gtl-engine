#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ActorComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "GameFramework/World.h"
#include "Core/Delegates/Delegate.h"
#include <Runtime/Script/ScriptComponent.h>

#include <algorithm>
#include <cctype>

DEFINE_CLASS(AActor, UObject)
REGISTER_FACTORY(AActor)

static FString TrimObjectNameText(const FString& Name)
{
    size_t Begin = 0;
    while (Begin < Name.size() && std::isspace(static_cast<unsigned char>(Name[Begin])) != 0)
    {
        ++Begin;
    }

    size_t End = Name.size();
    while (End > Begin && std::isspace(static_cast<unsigned char>(Name[End - 1])) != 0)
    {
        --End;
    }

    if (Begin >= End)
    {
        return "";
    }
    return Name.substr(Begin, End - Begin);
}

static bool ParseObjectNameNumber(const FString& Text, int32& OutNumber)
{
    if (Text.empty())
    {
        return false;
    }

    int32 Value = 0;
    for (char Ch : Text)
    {
        if (!std::isdigit(static_cast<unsigned char>(Ch)))
        {
            return false;
        }
        Value = Value * 10 + (Ch - '0');
    }

    OutNumber = Value;
    return true;
}

static bool SplitGeneratedObjectNameSuffix(const FString& Name, FString& OutBaseName, int32& OutNumber)
{
    const FString TrimmedName = TrimObjectNameText(Name);
    if (TrimmedName.empty())
    {
        return false;
    }

    size_t NumberBegin = TrimmedName.size();
    while (NumberBegin > 0 && std::isdigit(static_cast<unsigned char>(TrimmedName[NumberBegin - 1])) != 0)
    {
        --NumberBegin;
    }

    if (NumberBegin == TrimmedName.size() || NumberBegin == 0 || TrimmedName[NumberBegin - 1] != '_')
    {
        return false;
    }

    if (ParseObjectNameNumber(TrimmedName.substr(NumberBegin), OutNumber))
    {
        OutBaseName = TrimObjectNameText(TrimmedName.substr(0, NumberBegin - 1));
        return !OutBaseName.empty();
    }
    return false;
}

static FString StripGeneratedObjectNameSuffixes(const FString& Name)
{
    FString BaseName = TrimObjectNameText(Name);
    for (;;)
    {
        FString NextBaseName;
        int32 IgnoredNumber = 0;
        if (!SplitGeneratedObjectNameSuffix(BaseName, NextBaseName, IgnoredNumber))
        {
            return BaseName;
        }
        BaseName = NextBaseName;
    }
}

AActor::~AActor()
{
    if (OwningWorld != nullptr)
    {
        OwningWorld->GetSpatialIndex().UnregisterActor(this);
        OwningWorld = nullptr;
    }

	TArray<UActorComponent*> CopyComponents = OwnedComponents;

    for (auto* Comp : CopyComponents)
    {
        RemoveComponent(Comp);
    }

    OwnedComponents.clear();
    RootComponent = nullptr;
}

FString AActor::MakeUniqueComponentName(const UActorComponent* TargetComponent, const FString& RequestedName, bool bAlwaysAppendNumber) const
{
    FString BaseName = StripGeneratedObjectNameSuffixes(RequestedName);
    if (BaseName.empty())
    {
        BaseName = TargetComponent && TargetComponent->GetTypeInfo() ? TargetComponent->GetTypeInfo()->name : "UActorComponent";
    }

    bool bRequestedNameTaken = false;
    int32 HighestSuffix = 0;
    const FString RequestedCleanName = TrimObjectNameText(RequestedName);
    for (const UActorComponent* Component : OwnedComponents)
    {
        if (!Component || Component == TargetComponent)
        {
            continue;
        }

        const FString ExistingName = TrimObjectNameText(Component->GetFName().ToString());
        if (ExistingName == RequestedCleanName)
        {
            bRequestedNameTaken = true;
        }

        FString ExistingBaseName;
        int32 ExistingSuffix = 0;
        if (SplitGeneratedObjectNameSuffix(ExistingName, ExistingBaseName, ExistingSuffix)
            && StripGeneratedObjectNameSuffixes(ExistingBaseName) == BaseName)
        {
            HighestSuffix = std::max(HighestSuffix, ExistingSuffix);
        }
    }

    if (!bAlwaysAppendNumber && !RequestedCleanName.empty() && !bRequestedNameTaken)
    {
        return RequestedCleanName;
    }

    int32 Suffix = std::max(HighestSuffix + 1, 1);
    FString Candidate;
    bool bTaken = false;
    do
    {
        Candidate = BaseName + "_" + std::to_string(Suffix++);
        bTaken = false;
        for (const UActorComponent* Component : OwnedComponents)
        {
            if (Component && Component != TargetComponent && Component->GetFName() == FName(Candidate))
            {
                bTaken = true;
                break;
            }
        }
    }
    while (bTaken);

    return Candidate;
}

/* 액터의 계층 구조를 복원하기 위해 자식들을 재귀적으로 순회합니다. */
static USceneComponent* DuplicateSubTree(
    USceneComponent* OriginalComp, AActor* NewActor, TArray<UActorComponent*>& OwnedComponents,
    TMap<USceneComponent*, USceneComponent*>& OutCompMap)
{
    if (!OriginalComp)
        return nullptr;

    // 현재 노드(부모) 복제
    USceneComponent* DuplicatedComp = Cast<USceneComponent>(OriginalComp->Duplicate());
    if (!DuplicatedComp)
        return nullptr; // 에디터 전용 등이면 nullptr 반환됨

    // 소유자 등록
    DuplicatedComp->SetOwner(NewActor);
    OwnedComponents.push_back(DuplicatedComp);

    // 원본 → 복제본 매핑 등록 (MovementComponent의 UpdatedComponent 연결에 사용)
    OutCompMap[OriginalComp] = DuplicatedComp;

    // 자식들을 재귀적으로 순회하며 방금 복제된 자신(DuplicatedComp)에게 바로 Attach
    for (USceneComponent* Child : OriginalComp->GetChildren())
    {
        USceneComponent* DuplicatedChild = DuplicateSubTree(Child, NewActor, OwnedComponents, OutCompMap);
        if (DuplicatedChild)
        {
            DuplicatedChild->AttachToComponent(DuplicatedComp);
        }
    }

    return DuplicatedComp;
}

void AActor::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    PendingActorLocation = GetActorLocation();
    PendingActorRotation = GetActorRotation();
    PendingActorScale = GetActorScale();

    OutProps.push_back({ "Location", EPropertyType::Vec3, &PendingActorLocation });
    OutProps.push_back({ "Rotation", EPropertyType::Vec3, &PendingActorRotation });
    OutProps.push_back({ "Scale", EPropertyType::Vec3, &PendingActorScale });
    OutProps.push_back({ "Visible", EPropertyType::Bool, &bVisible });
    OutProps.push_back({ "Active", EPropertyType::Bool, &bIsActive });
    OutProps.push_back({ "Tick In Editor", EPropertyType::Bool, &bTickInEditor });
}

void AActor::PostEditProperty(const char* PropertyName)
{
    if (!PropertyName)
    {
        return;
    }

    const FString Name = PropertyName;
    if (Name == "Location")
    {
        SetActorLocation(PendingActorLocation);
    }
    else if (Name == "Rotation")
    {
        SetActorRotation(PendingActorRotation);
    }
    else if (Name == "Scale")
    {
        SetActorScale(PendingActorScale);
    }
}

void AActor::PostDuplicate(UObject* Original)
{
    UObject::PostDuplicate(Original);

    AActor* OrigActor = static_cast<AActor*>(Original);

    OwnedComponents.clear();
    Tags = OrigActor->Tags;

    // MovementComponent 등 일반 컴포넌트들의 참조를 복원하기 위한 맵을 선언합니다.
    TMap<USceneComponent*, USceneComponent*> ComponentMap;

    // 1. RootComponent부터 씬 컴포넌트 트리 전체를 복제하여 조립합니다.
    if (OrigActor->RootComponent)
    {
        SetRootComponent(DuplicateSubTree(OrigActor->RootComponent, this, OwnedComponents, ComponentMap));
    }

    // 2. SceneComponent가 아닌 일반 컴포넌트들을 복제하기 위해 배열을 순회합니다.
    for (UActorComponent* OriginalComp : OrigActor->OwnedComponents)
    {
        if (!OriginalComp)
            continue;

        // 씬 컴포넌트는 처리했으므로 건너뜁니다.
        if (OriginalComp->IsA<USceneComponent>())
            continue;

        UActorComponent* DuplicatedComp = Cast<UActorComponent>(OriginalComp->Duplicate());
        if (!DuplicatedComp)
            continue;

        DuplicatedComp->SetOwner(this);
        OwnedComponents.push_back(DuplicatedComp);

        // MovementComponent도 기존에 제어하던 컴포넌트를 제어하도록 연결해줍니다.
        if (UMovementComponent* OriginalMoveComp = Cast<UMovementComponent>(OriginalComp))
        {
            UMovementComponent* DuplicatedMoveComp = Cast<UMovementComponent>(DuplicatedComp);
            USceneComponent* OldTarget = OriginalMoveComp->GetUpdatedComponent();

            if (OldTarget && ComponentMap.find(OldTarget) != ComponentMap.end())
            {
                DuplicatedMoveComp->SetUpdatedComponent(ComponentMap[OldTarget]);
            }
            else
            {
                DuplicatedMoveComp->SetUpdatedComponent(GetRootComponent());
            }
        }
    }

    bPrimitiveCacheDirty = true;

	// Editor World -> PIE World 복사 후 다시 Register
	for (UActorComponent* Comp : OwnedComponents)
    {
        NotifyComponentRegistered(Comp);
    }
}

void AActor::Serialize(FArchive& Ar)
{
	Ar.BeginObject(std::to_string(GetUUID()));
	UObject::Serialize(Ar);
	Ar << "Visible" << bVisible;
	Ar << "Editor Only" << bTickInEditor;
    Ar << "Tags" << Tags;
	Ar.EndObject();

	for (UActorComponent* Comp : OwnedComponents)
	{
		Ar.BeginObject(std::to_string(Comp->GetUUID()));
		Comp->Serialize(Ar);
		Ar.EndObject();
	}
}

UActorComponent* AActor::AddComponentByClass(const FTypeInfo* Class)
{
    if (!Class)
        return nullptr;

    UObject* Obj = FObjectFactory::Get().Create(Class->name);
    if (!Obj)
        return nullptr;

    UActorComponent* Comp = Cast<UActorComponent>(Obj);
    if (!Comp)
    {
        UObjectManager::Get().DestroyObject(Obj);
        return nullptr;
    }

    Comp->SetOwner(this);
    Comp->SetFName(FName(MakeUniqueComponentName(Comp, Class->name, true)));
    OwnedComponents.push_back(Comp);
    bPrimitiveCacheDirty = true;
    NotifyComponentRegistered(Comp);
    return Comp;
}

void AActor::RegisterComponent(UActorComponent* Comp)
{
    if (!Comp)
        return;

    auto it = std::find(OwnedComponents.begin(), OwnedComponents.end(), Comp);
    if (it == OwnedComponents.end())
    {
        Comp->SetOwner(this);
        Comp->SetFName(FName(MakeUniqueComponentName(Comp, Comp->GetFName().ToString(), false)));
        OwnedComponents.push_back(Comp);
        bPrimitiveCacheDirty = true;
        NotifyComponentRegistered(Comp);
    }
}

void AActor::RemoveComponent(UActorComponent* Component)
{
    if (!Component)
        return;

    NotifyComponentUnregistered(Component);

    auto it = std::find(OwnedComponents.begin(), OwnedComponents.end(), Component);
    if (it != OwnedComponents.end())
    {
        OwnedComponents.erase(it);
        bPrimitiveCacheDirty = true;
    }

    // RootComponent가 제거되면 nullptr로
    if (RootComponent == Component)
        RootComponent = nullptr;
	
    UObjectManager::Get().DestroyObject(Component);
}

void AActor::SetVisible(bool Visible)
{
    if (bVisible == Visible)
    {
        return;
    }

    bVisible = Visible;
    MarkPrimitiveComponentsDirty();
}

namespace
{
    FString TrimActorTag(const FString& Value)
    {
        const auto IsSpace = [](unsigned char Ch)
        {
            return std::isspace(Ch) != 0;
        };

        auto Begin = std::find_if_not(Value.begin(), Value.end(), IsSpace);
        auto End = std::find_if_not(Value.rbegin(), Value.rend(), IsSpace).base();
        if (Begin >= End)
        {
            return {};
        }
        return FString(Begin, End);
    }
}

void AActor::AddTag(const FString& Tag)
{
    const FString CleanTag = TrimActorTag(Tag);
    if (CleanTag.empty() || HasTag(CleanTag))
    {
        return;
    }

    Tags.push_back(CleanTag);
}

void AActor::RemoveTag(const FString& Tag)
{
    const FString CleanTag = TrimActorTag(Tag);
    Tags.erase(
        std::remove(Tags.begin(), Tags.end(), CleanTag),
        Tags.end());
}

bool AActor::HasTag(const FString& Tag) const
{
    const FString CleanTag = TrimActorTag(Tag);
    return std::find(Tags.begin(), Tags.end(), CleanTag) != Tags.end();
}

void AActor::ClearTags()
{
    Tags.clear();
}

FString AActor::GetTagsText() const
{
    FString Result;
    for (size_t Index = 0; Index < Tags.size(); ++Index)
    {
        if (Index > 0)
        {
            Result += ", ";
        }
        Result += Tags[Index];
    }
    return Result;
}

void AActor::SetTagsFromText(const FString& InTagsText)
{
    Tags.clear();

    size_t Start = 0;
    while (Start <= InTagsText.size())
    {
        const size_t Comma = InTagsText.find(',', Start);
        const size_t End = (Comma == FString::npos) ? InTagsText.size() : Comma;
        AddTag(InTagsText.substr(Start, End - Start));

        if (Comma == FString::npos)
        {
            break;
        }
        Start = Comma + 1;
    }
}

void AActor::SetWorld(UWorld* World)
{
    if (OwningWorld == World)
    {
        return;
    }

    if (OwningWorld != nullptr)
    {
        OwningWorld->GetSpatialIndex().UnregisterActor(this);
    }

    OwningWorld = World;

    if (OwningWorld != nullptr)
    {
        OwningWorld->GetSpatialIndex().RegisterActor(this);
    }
}

void AActor::SetRootComponent(USceneComponent* Comp)
{
    if (!Comp)
        return;
    RootComponent = Comp;
}

FVector AActor::GetActorLocation() const
{
    if (RootComponent)
    {
        return RootComponent->GetWorldLocation();
    }
    return FVector(0, 0, 0);
}

void AActor::SetActorLocation(const FVector& NewLocation)
{
    PendingActorLocation = NewLocation;

    if (RootComponent)
    {
        RootComponent->SetWorldLocation(NewLocation);
    }
}

void AActor::BeginPlay()
{
    for (UActorComponent* Component : OwnedComponents)
    {
        if (Component)
        {
            Component->BeginPlay();
        }
    }
}

void AActor::Tick(float DeltaTime)
{
    for (UActorComponent* Component : OwnedComponents)
    {
        if (Component && Component->IsActive())
        {
            Component->ExecuteTick(DeltaTime);
        }
    }
}

void AActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    for (UActorComponent* Component : OwnedComponents)
    {
        if (Component)
        {
            Component->EndPlay();
        }
    }
}

void AActor::NotifyComponentRegistered(UActorComponent* Component)
{
    Component->OnRegister();
    PostComponentRegistered(Component);
    if (Component == nullptr || OwningWorld == nullptr)
    {
        return;
    }

    UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
    if (Primitive == nullptr)
    {
        return;
    }

    OwningWorld->GetSpatialIndex().RegisterPrimitive(Primitive);
    OwningWorld->GetSpatialIndex().FlushDirtyBounds();
}

void AActor::NotifyComponentUnregistered(UActorComponent* Component)
{
    Component->OnUnregister();
    PostComponentUnregistered(Component);
    if (Component == nullptr || OwningWorld == nullptr)
    {
        return;
    }

    UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
    if (Primitive == nullptr)
    {
        return;
    }

    OwningWorld->GetSpatialIndex().UnregisterPrimitive(Primitive);
}

void AActor::MarkPrimitiveComponentsDirty()
{
    if (OwningWorld == nullptr)
    {
        return;
    }

    for (UPrimitiveComponent* Primitive : GetPrimitiveComponents())
    {
        OwningWorld->GetSpatialIndex().MarkPrimitiveDirty(Primitive);
    }
}

const TArray<UPrimitiveComponent*>& AActor::GetPrimitiveComponents() const
{
    if (bPrimitiveCacheDirty)
    {
        PrimitiveCache.clear();
        for (UActorComponent* Comp : OwnedComponents)
        {
            if (Comp && Comp->IsA<UPrimitiveComponent>())
            {
                PrimitiveCache.emplace_back(static_cast<UPrimitiveComponent*>(Comp));
            }
        }
        bPrimitiveCacheDirty = false;
    }
    return PrimitiveCache;
}


bool AActor::IsOverlappingActor(const AActor* Other) const
{
	for (UActorComponent* OwnedComp : OwnedComponents)
    {
        if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(OwnedComp))
        {
            if ((PrimComp->GetOverlapInfos().size() > 0) && PrimComp->IsOverlappingActor(Other))
            {
                // found one, finished
                return true;
            }
        }
	}

    return false;
}

void AActor::PostComponentRegistered(UActorComponent* Comp)
{
    UShapeComponent* ShapeComp = Cast<UShapeComponent>(Comp);

    if (ShapeComp)
    {
		OnComponentHitHandleId = ShapeComp->OnComponentHit.AddDynamic(this, &ThisClass::OnHit);
		OnComponentBeginOverlapHandleId = ShapeComp->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::OnBeginOverlap);
		OnComponentEndOverlapHandleId = ShapeComp->OnComponentEndOverlap.AddDynamic(this, &ThisClass::OnEndOverlap);
    }
}

void AActor::PostComponentUnregistered(UActorComponent* Comp)
{
    UShapeComponent* ShapeComp = Cast<UShapeComponent>(Comp);

    if (ShapeComp)
    {
        ShapeComp->OnComponentHit.Remove(OnComponentHitHandleId);
        ShapeComp->OnComponentBeginOverlap.Remove(OnComponentBeginOverlapHandleId);
        ShapeComp->OnComponentEndOverlap.Remove(OnComponentEndOverlapHandleId);
    }
}

void AActor::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    for (UActorComponent* Component : GetComponents())
    {
        if (UScriptComponent* ScriptComponent = Cast<UScriptComponent>(Component))
        {
            ScriptComponent->OnHit(
                HitComponent,
                OtherActor,
                OtherComp,
                NormalImpulse,
                Hit);
        }
    }
}
void AActor::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    for (UActorComponent* Component : GetComponents())
    {
        if (UScriptComponent* ScriptComponent = Cast<UScriptComponent>(Component))
        {
            ScriptComponent->OnBeginOverlap(
                OverlappedComponent,
                OtherActor,
                OtherComp,
                OtherBodyIndex,
                bFromSweep,
                SweepResult);
        }
    }
}

void AActor::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    for (UActorComponent* Component : GetComponents())
    {
        if (UScriptComponent* ScriptComponent = Cast<UScriptComponent>(Component))
        {
            ScriptComponent->OnEndOverlap(
                OverlappedComponent,
                OtherActor,
                OtherComp,
                OtherBodyIndex,
                bFromSweep,
                SweepResult);
        }
    }
}
