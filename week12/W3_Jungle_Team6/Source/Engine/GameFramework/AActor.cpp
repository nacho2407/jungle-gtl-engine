#include "GameFramework/AActor.h"

DEFINE_CLASS(AActor, UObject)
REGISTER_FACTORY(AActor)

AActor::~AActor() {
	for (auto* Comp : Components) {
		delete Comp;
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