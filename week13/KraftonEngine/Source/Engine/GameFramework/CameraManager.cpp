#include "CameraManager.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UCameraManager, UObject)

void UCameraManager::RegisterCamera(UCameraComponent* Camera)
{
	if (Camera)
	{
		RegisteredCameras.insert(Camera);
	}
}

void UCameraManager::UnregisterCamera(UCameraComponent* Camera)
{
	if (Camera)
	{
		RegisteredCameras.erase(Camera);
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
	if (!RegisteredCameras.empty())
	{
		SetActiveCamera(*RegisteredCameras.begin());
		Possess(*RegisteredCameras.begin());
	}
}
