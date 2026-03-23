#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ActorComponent.h"

DEFINE_CLASS(AActor, UObject)
REGISTER_FACTORY(AActor)

AActor::~AActor() {
	for (auto* Comp : Components) {
		UObjectManager::Get().DestroyObject(Comp);
	}

	Components.clear();
	RootComponent = nullptr;
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

void AActor::RegisterComponentRecursive(USceneComponent* Comp) {
	if (!Comp) return;

	// Avoid duplicates
	auto it = std::find(Components.begin(), Components.end(), Comp);
	if (it == Components.end()) {
		Comp->SetOwner(this);
		Components.push_back(Comp);
	}

	for (USceneComponent* Child : Comp->GetChildren()) {
		RegisterComponentRecursive(Child);
	}
}

void AActor::Tick(float DeltaTime)
{
	for (USceneComponent* Comp : Components)
	{
		if (UActorComponent* ActorComp = dynamic_cast<UActorComponent*>(Comp))
		{
			ActorComp->ExecuteTick(DeltaTime);
		}
	}
}

const TArray<UPrimitiveComponent*>& AActor::GetPrimitiveComponents() const
{
	// 컴포넌트 리스트가 바뀌었을 때만 딱 한 번 실행
	if (bComponentsDirty)
	{
		PrimitiveCache.clear();

		// 이미 가지고 있는 Components 배열을 활용합니다. (재귀 호출 필요 없음!)
		for (USceneComponent* Comp : Components)
		{
			if (auto* Primitive = dynamic_cast<UPrimitiveComponent*>(Comp))
			{
				PrimitiveCache.emplace_back(Primitive);
			}
		}
		bComponentsDirty = false;
	}
	return PrimitiveCache;
}