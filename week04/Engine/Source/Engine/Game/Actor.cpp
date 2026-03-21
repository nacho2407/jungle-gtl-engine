#include "Actor.h"

AActor::AActor() = default;

bool AActor::IsPickable() const { return bPickable; }

void AActor::SetPickable(bool bInPickable) { bPickable = bInPickable; }