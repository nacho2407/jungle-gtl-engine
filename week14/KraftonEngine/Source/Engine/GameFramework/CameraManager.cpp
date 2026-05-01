#include "CameraManager.h"
#include "Component/CameraComponent.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include <algorithm>

IMPLEMENT_CLASS(UCameraManager, UObject)

void UCameraManager::RegisterCamera(UCameraComponent* Camera)
{
	if (Camera)
	{
		if (RegisteredCameras.insert(Camera).second)
		{
			RegisteredCameraOrder.push_back(Camera);
		}
	}
}

void UCameraManager::UnregisterCamera(UCameraComponent* Camera)
{
	if (Camera)
	{
		RegisteredCameras.erase(Camera);
		RegisteredCameraOrder.erase(
			std::remove(RegisteredCameraOrder.begin(), RegisteredCameraOrder.end(), Camera),
			RegisteredCameraOrder.end());
		if (ActiveCamera == Camera)
		{
			ActiveCamera = nullptr;
		}
		if (PossessedCamera == Camera)
		{
			PossessedCamera = nullptr;
		}
	}
}

void UCameraManager::AutoPossessDefaultCamera()
{
	if (!RegisteredCameraOrder.empty())
	{
		SetActiveCamera(RegisteredCameraOrder.front());
		Possess(RegisteredCameraOrder.front());
	}
}

// 현재는 Actor당 카메라 최대 2개만 가능
bool UCameraManager::ToggleActiveCameraForActor(const FString& ActorName)
{
	TArray<UCameraComponent*> ActorCameras;
	for (UCameraComponent* Camera : RegisteredCameraOrder)
	{
		if (Camera && Camera->GetOwner()
			&& Camera->GetOwner()->GetFName().ToString() == ActorName)
		{
			ActorCameras.push_back(Camera);
		}
	}

	if (ActorCameras.empty())
	{
		return false;
	}

	if (ActorCameras.size() == 1)
	{
		SetActiveCamera(ActorCameras.front());
		Possess(ActorCameras.front());
		return true;
	}

	UCameraComponent* NextCamera = ActorCameras[0];
	if (ActiveCamera == ActorCameras[0])
	{
		NextCamera = ActorCameras[1];
	}

	SetActiveCamera(NextCamera);
	Possess(NextCamera);
	return true;
}

bool UCameraManager::ToggleActiveCameraForActor(const AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}

	TArray<UCameraComponent*> ActorCameras;
	for (UCameraComponent* Camera : RegisteredCameraOrder)
	{
		if (Camera && Camera->GetOwner() == Actor)
		{
			ActorCameras.push_back(Camera);
		}
	}

	if (ActorCameras.empty())
	{
		return false;
	}

	if (ActorCameras.size() == 1)
	{
		SetActiveCamera(ActorCameras.front());
		Possess(ActorCameras.front());
		return true;
	}

	UCameraComponent* NextCamera = ActorCameras[0];
	if (ActiveCamera == ActorCameras[0])
	{
		NextCamera = ActorCameras[1];
	}

	SetActiveCamera(NextCamera);
	Possess(NextCamera);
	return true;
}
