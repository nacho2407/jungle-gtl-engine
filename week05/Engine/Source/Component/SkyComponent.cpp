#include "SkyComponent.h"

#include "Actor/Actor.h"
#include "Obj/ObjManager.h"
#include "Primitive/PrimitiveSky.h"
#include "Object/Class.h"
#include "Renderer/MaterialManager.h"
#include "CameraComponent.h"
#include "Camera/Camera.h"
#include "World/World.h"

IMPLEMENT_RTTI(USkyComponent, UStaticMeshComponent)

void USkyComponent::PostConstruct()
{
	bDrawDebugBounds = false;
	bCanEverTick = true;
	SetStaticMesh(FObjManager::GetPrimitiveSky());

	std::shared_ptr<FMaterial> SkyMat = FMaterialManager::Get().FindByName("M_Sky");
	if (SkyMat)
	{
		SetMaterial(0, SkyMat);
	}

	FTransform T = GetRelativeTransform();
	T.SetScale3D({ 2000.0f, 2000.0f, 2000.0f });
	SetRelativeTransform(T);
}

void USkyComponent::Tick(float DeltaTime)
{
	UStaticMeshComponent::Tick(DeltaTime);

	if (UWorld* World = GetOwner()->GetWorld())
	{
		if (UCameraComponent* CameraComp = World->GetActiveCameraComponent())
		{
			FTransform T = GetRelativeTransform();
			T.SetTranslation(CameraComp->GetCamera()->GetPosition());
			SetRelativeTransform(T);
		}
	}
}

FBoxSphereBounds USkyComponent::GetWorldBounds() const
{
	return { FVector(0,0,0), FLT_MAX, FVector(FLT_MAX,FLT_MAX,FLT_MAX) };
}
