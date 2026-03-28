#include "Editor/ObjViewerEngine.h"

#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Core/Logging/Stats.h"
#include "Editor/ObjViewerRenderPipeline.h"
#include "Engine/GameFramework/PrimitiveActors.h"
#include "Engine/GameFramework/World.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "GameFramework/World.h"

DEFINE_CLASS(UObjViewerEngine, UEngine)
REGISTER_FACTORY(UObjViewerEngine)

void UObjViewerEngine::Init(FWindowsWindow* InWindow)
{
	// 엔진 공통 초기화 (Renderer, D3D, 싱글턴 등)
	UEngine::Init(InWindow);
	
	// ObjViewer 전용 setting 파일 ObjViewer.ini 불러오기
	FObjViewerSettings::Get().LoadFromFile(FObjViewerSettings::GetDefaultSettingsPath());
	
	MainPanel.Create(Window, Renderer, this);

	// 프리뷰 전용 월드 생성
	if (WorldList.empty())
	{
		CreateWorldContext(EWorldType::Preview, FName("ObjViewerPath"), "ObjViewerWorld");
	}
	SetActiveWorld(WorldList[0].ContextHandle);
	GetWorld()->InitWorld();

	// 뷰포트 및 카메라 설정
	ViewportClient.SetSettings(&FObjViewerSettings::Get());
	ViewportClient.Initialize(InWindow);
	ViewportClient.SetViewportSize(InWindow->GetWidth(), InWindow->GetHeight());
	ViewportClient.SetWorld(GetWorld());

	// 엔진 시스템에 활성 카메라 등록
	ViewportClient.CreateCamera();
	ViewportClient.ResetCamera();

	// Obj Viewer Render Pipeline 세팅
	SetRenderPipeline(std::make_unique<FObjViewerRenderPipeline>(this, Renderer));
}

void UObjViewerEngine::Shutdown()
{
	FObjViewerSettings::Get().SaveToFile(FObjViewerSettings::GetDefaultSettingsPath());

	// 엔진 공통 해제 (Renderer, D3D 등)
	UEngine::Shutdown();
}

void UObjViewerEngine::BeginPlay()
{
	// World Context Handle을 기반으로 현재 실행 중인 월드 탐색
	UWorld* World = GetWorld();
	if (!World)
		return;

	// 테스트용 ACubeActor 생성
	ACubeActor* PreviewCube = World->SpawnActor<ACubeActor>();
	PreviewCube->SetActorLocation(FVector(0, 0, 0));
	PreviewCube->InitDefaultComponents();

	// ViewportClient의 카메라 가져오기
	UCameraComponent* MainCamera = ViewportClient.GetCamera();
	if (MainCamera)
	{
		MainCamera->SetWorldLocation(FVector(2.0f, 3.0f, 3.0f));
		MainCamera->LookAt(FVector(0.0f, 0.0f, 0.0f));
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

// UI 렌더링
void UObjViewerEngine::RenderUI(float DeltaTime)
{
	MainPanel.Render(DeltaTime);
}

// 창 크기가 바뀔 때마다 카메라의 비율(Aspect Ratio) 다시 계산
void UObjViewerEngine::OnWindowResized(uint32 Width, uint32 Height)
{
	UEngine::OnWindowResized(Width, Height);
	ViewportClient.SetViewportSize(Window->GetWidth(), Window->GetHeight());
}
