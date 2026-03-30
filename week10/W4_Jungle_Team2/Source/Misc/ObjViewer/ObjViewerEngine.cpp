#include "Misc/ObjViewer/ObjViewerEngine.h"
#include "Misc/ObjViewer/ObjViewerRenderPipeline.h"

#include "Component/CameraComponent.h"
#include "Core/Logging/Stats.h"
#include "Core/ResourceManager.h"
#include "Engine/Component/StaticMeshComponent.h"
#include "Engine/GameFramework/PrimitiveActors.h"
#include "Engine/GameFramework/World.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "GameFramework/World.h"
#include "ImGui/imgui.h"
#include "Viewport/ViewportCamera.h"

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

	// 프리뷰 액터 및 컴포넌트를 생성해 멤버 변수에 저장한다.
	AActor* PreviewActor = GetWorld()->SpawnActor<AActor>();
	auto* MeshComp = PreviewActor->AddComponent<UStaticMeshComponent>();
	PreviewMeshComponent = MeshComp;
	PreviewActor->SetRootComponent(MeshComp);

	// 뷰포트 및 카메라 설정
	ViewportClient.SetSettings(&FObjViewerSettings::Get());
	ViewportClient.Initialize(InWindow);
	ViewportClient.SetViewportRect(0, 0, InWindow->GetWidth(), InWindow->GetHeight());
	ViewportClient.SetWorld(GetWorld());

	// 엔진 시스템에 활성 카메라 등록
	ViewportClient.CreateCamera();
	ViewportClient.ResetCamera();

	// Obj Viewer Render Pipeline 세팅
	SetRenderPipeline(std::make_unique<FObjViewerRenderPipeline>(this, Renderer));
}

void UObjViewerEngine::Shutdown()
{
	FString SavePath = FPaths::ToRelativeString(FPaths::ToWide(FObjViewerSettings::GetDefaultSettingsPath()));
	FObjViewerSettings::Get().SaveToFile(SavePath);

	// 엔진 공통 해제 (Renderer, D3D 등)
	UEngine::Shutdown();
}

void UObjViewerEngine::BeginPlay()
{
	// World Context Handle을 기반으로 현재 실행 중인 월드 탐색
	UWorld* World = GetWorld();
	if (!World)
		return;

    // 카메라 세팅은 ViewportClient에게 온전히 위임
    if (FViewportCamera* MainCamera = ViewportClient.GetCamera())
    {
        World->SetActiveCamera(MainCamera);
    }

	ViewportClient.ResetCamera();

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
	// TODO-VIEWER: Slate 구조 개편 후 다시 작성하기
	UEngine::OnWindowResized(Width, Height);
    ViewportClient.SetViewportRect(0, 0, Window->GetWidth(), Window->GetHeight());
}
