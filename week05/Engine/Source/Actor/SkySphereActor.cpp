#include "SkySphereActor.h"
#include "Component/SkyComponent.h"
#include "Object/ObjectFactory.h"
#include "Scene/Scene.h"
#include "Component/CameraComponent.h"
#include "Renderer/MaterialManager.h"
#include "Camera/Camera.h"
#include "Object/Class.h"


IMPLEMENT_RTTI(ASkySphereActor, AActor)

void ASkySphereActor::PostSpawnInitialize()
{
	SkyComponent = FObjectFactory::ConstructObject<USkyComponent>(this);
	AddOwnedComponent(SkyComponent);
	std::shared_ptr<FMaterial> SkyMat = FMaterialManager::Get().FindByName("M_Sky");
	if (SkyMat)
	{
		SkyComponent->SetMaterial(SkyMat.get());
	}
	// SceneComponent need SetRelativeScale3D,  SetWorldLocation
	if (USceneComponent* Root = GetRootComponent())
	{
		FTransform T = Root->GetRelativeTransform();
		T.SetScale3D({ 2000.0f, 2000.0f, 2000.0f });
		Root->SetRelativeTransform(T);
	}

	AActor::PostSpawnInitialize();
}

void ASkySphereActor::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);
	if (UWorld* World = GetWorld())
	{
		if (UCameraComponent* CameraComp = World->GetActiveCameraComponent())
		{
			if (USceneComponent* Root = GetRootComponent())
			{
				FTransform T = Root->GetRelativeTransform();

				T.SetTranslation(CameraComp->GetCamera()->GetPosition());
				T.SetScale3D({ 2000.0f, 2000.0f, 2000.0f }); // tempcode need a fix
				Root->SetRelativeTransform(T);
			}
		}
	}
}
