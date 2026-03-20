/*#include "CollisionManager.h"
#include <limits>
#include <algorithm>

void UCollisionManager::AddCollider(UCollider* Collider)
{
	if (Collider != nullptr)
	{
		Colliders.push_back(Collider);
	}
}

void UCollisionManager::RemoveCollider(UCollider* collider)
{
	auto it = std::find(Colliders.begin(), Colliders.end(), collider);
	if (it != Colliders.end())
	{
		Colliders.erase(it);
	}
}

UCollider* UCollisionManager::Raycast(const FRay& Ray, float& OutDistance)
{
	UCollider* closetCollider = nullptr;
	float minDistance = FLT_MAX;

	for (UCollider* collider : Colliders)
	{
		float hitDistance = 0.0f;
		if (collider->Raycast(Ray, hitDistance))
		{
			if(hitDistance < minDistance)
			{
				minDistance = hitDistance;
				closetCollider = collider;
			}
		}
	}

	OutDistance = minDistance;
	return closetCollider;
}*/