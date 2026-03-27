#include "Editor/ObjViewerEngine.h"

#include "Editor/EditorRenderPipeline.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/GameFramework/World.h"
#include "Engine/GameFramework/PrimitiveActors.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Engine/Render/Renderer/DefaultRenderPipeline.h"
#include "Component/GizmoComponent.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/World.h"
#include "Core/Stats.h"

DEFINE_CLASS(UObjViewerEngine, UEngine)
REGISTER_FACTORY(UObjViewerEngine)

void UObjViewerEngine::Init(FWindowsWindow* InWindow)
{
	// 엔진 공통 초기화 (Renderer, D3D, 싱글턴 등)
    UEngine::Init(InWindow);

	FEditorSettings::Get().LoadFromFile(FEditorSettings::GetDefaultSettingsPath());

	// 프리뷰 전용 월드 생성 (EWorldType::Preview 사용)
	if (WorldList.empty())
	{
		CreateWorldContext(EWorldType::Preview, FName("ObjViewerPath"), "ObjViewerWorld");
	}
	SetActiveWorld(WorldList[0].ContextHandle);
	GetWorld()->InitWorld();

	// 뷰포트 및 카메라 설정 (회색 화면 방지를 위해 크기 즉시 세팅)
	ViewportClient.SetSettings(&FEditorSettings::Get());
	ViewportClient.Initialize(InWindow);
	ViewportClient.SetViewportSize(InWindow->GetWidth(), InWindow->GetHeight());
	ViewportClient.SetWorld(GetWorld());
	ViewportClient.SetGizmo(SelectionManager.GetGizmo());
	ViewportClient.SetSelectionManager(&SelectionManager);

	// 엔진 시스템에 활성 카메라 등록
	ViewportClient.CreateCamera();
	ViewportClient.ResetCamera();
	GetWorld()->SetActiveCamera(ViewportClient.GetCamera());

	SetRenderPipeline(std::make_unique<FDefaultRenderPipeline>(this, Renderer));
}

void UObjViewerEngine::Shutdown()
{
	// 에디터 해제 (엔진보다 먼저)
	FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());
	SelectionManager.Shutdown();

	// 엔진 공통 해제 (Renderer, D3D 등)
	UEngine::Shutdown();
}

void UObjViewerEngine::BeginPlay()
{
	// World Context Handle을 기반으로 현재 실행 중인 월드 탐색
    UWorld* World = GetWorld();
    if (!World) return;

	// 테스트용 ACubeActor 생성
	for (int i = -1000; i <= 1000; i+= 50)
	{
		for (int j = -1000; j <= 1000; j+= 50)
		{
			for (int k = -1000; k <= 1000; k += 50)
			{
			ACubeActor* PreviewCube = World->SpawnActor<ACubeActor>();
			PreviewCube->SetActorLocation(FVector(i, j, k));
			PreviewCube->InitDefaultComponents();
			}
		}
	}

	// ViewportClient의 카메라 가져오기
	UCameraComponent* MainCamera = ViewportClient.GetCamera();
    if (MainCamera)
    {
        MainCamera->SetWorldLocation(FVector(2.0f, 3.0f, 3.0f));
		MainCamera->LookAt(FVector(0.0f, 0.0f, 0.0f));
        World->SetActiveCamera(MainCamera);
    }

	// 기본 조명 스폰
    // AStaticLightActor* Light = World->SpawnActor<AStaticLightActor>();
    
    World->BeginPlay();
}

void UObjViewerEngine::Tick(float DeltaTime)
{
    // 마우스 입력을 통한 카메라 회전 업데이트, 상위 Tick 호출
	ViewportClient.Tick(DeltaTime);
    UEngine::Tick(DeltaTime);
}

// 창 크기가 바뀔 때마다 카메라의 비율(Aspect Ratio) 다시 계산
void UObjViewerEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	UEngine::OnWindowResized(Width, Height);
	ViewportClient.SetViewportSize(Window->GetWidth(), Window->GetHeight());
}