#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ActorComponent.h"

DEFINE_CLASS(AActor, UObject)
REGISTER_FACTORY(AActor)

AActor::~AActor() {
	for (auto* Comp : OwnedComponents) {
		UObjectManager::Get().DestroyObject(Comp);
	}

	OwnedComponents.clear();
	RootComponent = nullptr;
}

AActor* AActor::Duplicate()
{
	AActor* NewActor = UObjectManager::Get().CreateObject<AActor>();

	// 멤버 변수들을 복사합니다.
	NewActor->SetVisible(this->IsVisible());
	NewActor->PendingActorLocation = this->PendingActorLocation;
	NewActor->bTickInEditor = this->bTickInEditor;
	NewActor->OwnedComponents = this->OwnedComponents;
	NewActor->RootComponent = this->RootComponent;

	// 컴포넌트들은 깊은 복사로 처리합니다.
	NewActor->DuplicateSubObjects();

	return NewActor;
}

/* 
* @brief 액터들이 가진 여러 컴포넌트는 부모-자식 관계가 있을 수 있습니다.
* 복제되는 컴포넌트들은 복제된 자기 부모 컴포넌트의 포인터가 어떤 값일지 모르기 때문에,
* 액터에서 복제할 때 이를 일일이 설정해 줘야 합니다. 
*/
AActor* AActor::DuplicateSubObjects()
{
	TMap<USceneComponent*, USceneComponent*> ComponentMap;

    for (int32 i = 0; i < OwnedComponents.size(); ++i)
    {
        UActorComponent* OriginalComp = OwnedComponents[i];
        if (OriginalComp)
        {
            UActorComponent* DuplicatedComp = OriginalComp->Duplicate();

            if (DuplicatedComp)
            {
                DuplicatedComp->SetOwner(this);
                OwnedComponents[i] = DuplicatedComp;

                USceneComponent* OriginalSceneComp = Cast<USceneComponent>(OriginalComp);
                if (OriginalSceneComp)
                {
                    USceneComponent* DuplicatedSceneComp = Cast<USceneComponent>(DuplicatedComp);
                    ComponentMap[OriginalSceneComp] = DuplicatedSceneComp;

                    if (OriginalComp == this->RootComponent)
                    {
                        this->SetRootComponent(DuplicatedSceneComp);
                    }
                }
            }
            else
            {
                OwnedComponents[i] = nullptr;
            }
        }
    }

    // nullptr로 처리된 컴포넌트는 에디터 전용이므로 제거합니다.
    OwnedComponents.erase(std::remove(OwnedComponents.begin(), OwnedComponents.end(), nullptr), OwnedComponents.end());

    // 씬 컴포넌트 간의 부모-자식 관계를 복원합니다.
    for (auto& Pair : ComponentMap) 
    {
        USceneComponent* OriginalSceneComp = Pair.first;
        USceneComponent* DuplicatedSceneComp = Pair.second;
        USceneComponent* OriginalParent = OriginalSceneComp->GetParent();

        if (OriginalParent && ComponentMap.find(OriginalParent) != ComponentMap.end()) 
        {
            DuplicatedSceneComp->AttachToComponent(ComponentMap[OriginalParent]);
        }
    }

    bPrimitiveCacheDirty = true;

	return this;
}

UActorComponent* AActor::AddComponentByClass(const FTypeInfo* Class) {
	if (!Class) return nullptr;

	UObject* Obj = FObjectFactory::Get().Create(Class->name);
	if (!Obj) return nullptr;

	UActorComponent* Comp = Cast<UActorComponent>(Obj);
	if (!Comp) {
		UObjectManager::Get().DestroyObject(Obj);
		return nullptr;
	}

	Comp->SetOwner(this);
	OwnedComponents.push_back(Comp);
	return Comp;
}

void AActor::RegisterComponent(UActorComponent* Comp) {
	if (!Comp) return;

	auto it = std::find(OwnedComponents.begin(), OwnedComponents.end(), Comp);
	if (it == OwnedComponents.end()) {
		Comp->SetOwner(this);
		OwnedComponents.push_back(Comp);
		bPrimitiveCacheDirty = true;
	}
}

void AActor::RemoveComponent(UActorComponent* Component) {
	if (!Component) return;

	auto it = std::find(OwnedComponents.begin(), OwnedComponents.end(), Component);
	if (it != OwnedComponents.end()) {
		OwnedComponents.erase(it);
		bPrimitiveCacheDirty = true;
	}

	// RootComponent가 제거되면 nullptr로
	if (RootComponent == Component)
		RootComponent = nullptr;

	UObjectManager::Get().DestroyObject(Component);
}

void AActor::SetRootComponent(USceneComponent* Comp) {
	if (!Comp) return;
	RootComponent = Comp;
}

FVector AActor::GetActorLocation() const {
	if (RootComponent) {
		return RootComponent->GetWorldLocation();
	}
	return FVector(0, 0, 0);
}

void AActor::SetActorLocation(const FVector& NewLocation) {
	PendingActorLocation = NewLocation;

	if (RootComponent) {
		RootComponent->SetWorldLocation(NewLocation);
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
