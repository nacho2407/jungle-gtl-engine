#include "World.h"

DEFINE_CLASS(UWorld, UObject)
REGISTER_FACTORY(UWorld)

UWorld::~UWorld() {
    if (!Actors.empty()) {
        EndPlay();
    }
}

/*
    TODO : World에서의 카메라 종속성을 제거
    -> 만일 이후에 카메라의 위치를 저장하고 싶다면,
    -> 카메라의 위치를 저장하는 방법을 고민해보아야 함.
    -> 현재는 카메라의 이동을 Editor에서만 처리하고 있음. (문제가 되어 제거함)
*/

void UWorld::InitWorld() {
    // Create a default camera and make it active
    //UCamera* DefaultCamera = UObjectManager::Get().CreateObject<UCamera>();
    //DefaultCamera->SetProjectionSettings(M_PI / 2.0f, 16.0 / 9.0f, 0.1f, 1000.0f);
    //ActiveCamera = DefaultCamera;
}

void UWorld::SetActiveCamera(UCamera* Cam)
{
    if (ActiveCamera == Cam)
    {
        return;
    }

    if (ActiveCamera)
    {
        UObjectManager::Get().DestroyObject(ActiveCamera);
    }

    ActiveCamera = Cam;
}

void UWorld::BeginPlay() {
    for (AActor* Actor : Actors) {
        if (Actor && !Actor->bPendingKill) {
            Actor->BeginPlay();
        }
    }
}

void UWorld::Tick(float DeltaTime) {
    // Refactor this to somewhere else
	//CameraControls(DeltaTime);

    for (AActor* Actor : Actors) {
        if (Actor && !Actor->bPendingKill) {
            Actor->Tick(DeltaTime);
        }
    }

    // Cleanup destroyed objects here
}

void UWorld::EndPlay() {
    for (AActor* Actor : Actors) {
        if (Actor && !Actor->bPendingKill) {
            Actor->EndPlay();
            UObjectManager::Get().DestroyObject(Actor);
        }
    }

    Actors.clear();

    // Cleanup
    if (ActiveCamera) {
        UObjectManager::Get().DestroyObject(ActiveCamera);
    }

    ActiveCamera = nullptr;
    bPendingKill = true;
    //UObjectManager::Get().CollectGarbage();
}

//  현재 입력은 Editor에서만 처리되고 있습니다.
//void UWorld::CameraControls(float DeltaTime) {
//	if (ActiveCamera) {
//        float cameraVelocity = 0.1f;
//        FVector move = FVector(0, 0, 0);
//        if (InputSystem::GetKey('W'))
//            move.Z += cameraVelocity;
//        if (InputSystem::GetKey('A'))
//            move.X -= cameraVelocity;
//        if (InputSystem::GetKey('S'))
//            move.Z -= cameraVelocity;
//        if (InputSystem::GetKey('D'))
//            move.X += cameraVelocity;
//
//        float cameraAngleVelocity = 0.02f;
//        FVector rotation = FVector(0, 0, 0);
//        if (InputSystem::GetKey(VK_UP))
//            rotation.Y += cameraAngleVelocity;
//        if (InputSystem::GetKey(VK_LEFT))
//            rotation.X -= cameraAngleVelocity;
//        if (InputSystem::GetKey(VK_DOWN))
//            rotation.Y -= cameraAngleVelocity;
//        if (InputSystem::GetKey(VK_RIGHT))
//            rotation.X += cameraAngleVelocity;
//
//        move *= DeltaTime;
//        rotation *= DeltaTime;
//
//        ActiveCamera->Move(move);
//        ActiveCamera->Rotate(rotation.X, rotation.Y);
//
//        int zoomspeed = 3;
//        int scroll = InputSystem::GetScrollNotches();
//        if (scroll != 0)
//        {
//            float newFOV = ActiveCamera->GetFOV()
//                - (scroll / 120.0f) * zoomspeed * DeltaTime;
//            newFOV = Clamp(newFOV, 1.f, M_PI / 2.f);
//            ActiveCamera->SetFOV(newFOV);
//        }
//	}
//}