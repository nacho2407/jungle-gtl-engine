//#include "ObjViewer/ObjViewerEngine.h"
//
//#include "ObjViewer/ObjViewerRenderPipeline.h"
//#include "Engine/Runtime/WindowsWindow.h"
//#include "GameFramework/World.h"
//#include "Object/ObjectFactory.h"
//#include "Mesh/ObjManager.h"
//#include "Component/StaticMeshComponent.h"
//#include "GameFramework/AActor.h"
//#include "Viewport/Viewport.h"
//
//IMPLEMENT_CLASS(UObjViewerEngine, UEngine)
//
//void UObjViewerEngine::Init(FWindowsWindow* InWindow)
//{
//	UEngine::Init(InWindow);
//
//	FObjManager::ScanMeshAssets();
//	FObjManager::ScanObjSourceFiles();
//
//	// ImGui 패널 초기화
//	Panel.Create(InWindow, Renderer, this);
//
//	// World
//	if (WorldList.empty())
//	{
//		CreateWorldContext(EWorldType::Game, FName("ObjViewer"));
//	}
//	SetActiveWorld(WorldList[0].ContextHandle);
//	GetWorld()->InitWorld();
//
//	// 뷰포트 클라이언트 + 오프스크린 RT
//	ViewportClient.Initialize(InWindow);
//	ViewportClient.CreateCamera();
//	ViewportClient.ResetCamera();
//
//	FViewport* VP = new FViewport();
//	VP->Initialize(Renderer.GetFD3DDevice().GetDevice(),
//		static_cast<uint32>(InWindow->GetWidth()),
//		static_cast<uint32>(InWindow->GetHeight()));
//	VP->SetClient(&ViewportClient);
//	ViewportClient.SetViewport(VP);
//
//	// ObjViewer 전용 렌더 경로
//	RenderPipeline = std::make_unique<FObjViewerRenderPipeline>(this, Renderer);
//}
//
//void UObjViewerEngine::Shutdown()
//{
//	ViewportClient.Release();
//	Panel.Release();
//
//	for (FWorldContext& Ctx : WorldList)
//	{
//		Ctx.World->EndPlay();
//		UObjectManager::Get().DestroyObject(Ctx.World);
//	}
//	WorldList.clear();
//	ActiveWorldHandle = FName::None;
//
//	UEngine::Shutdown();
//}
//
//void UObjViewerEngine::Tick(float DeltaTime)
//{
//	ViewportClient.Tick(DeltaTime);
//	Panel.Update();
//	UEngine::Tick(DeltaTime);
//}
//
//void UObjViewerEngine::Render(float DeltaTime)
//{
//	if (RenderPipeline)
//	{
//		RenderPipeline->Execute(DeltaTime, Renderer);
//		return;
//	}
//
//	UEngine::Render(DeltaTime);
//}
//
//void UObjViewerEngine::RenderUI(float DeltaTime)
//{
//	Panel.Render(DeltaTime);
//}
//
//void UObjViewerEngine::LoadPreviewMesh(const FString& MeshPath)
//{
//	UWorld* World = GetWorld();
//	if (!World) return;
//
//	// 기존 액터 모두 제거
//	TArray<AActor*> Actors = World->GetActors();
//	for (AActor* Actor : Actors)
//	{
//		World->DestroyActor(Actor);
//	}
//
//	// 메시 로드
//	ID3D11Device* Device = Renderer.GetFD3DDevice().GetDevice();
//	UStaticMesh* Mesh = FObjManager::LoadObjStaticMesh(MeshPath, Device);
//	if (!Mesh) return;
//
//	// 프리뷰 액터 생성
//	AActor* PreviewActor = World->SpawnActor<AActor>();
//	if (!PreviewActor) return;
//
//	UStaticMeshComponent* MeshComp = PreviewActor->AddComponent<UStaticMeshComponent>();
//	MeshComp->SetStaticMesh(Mesh);
//	PreviewActor->SetRootComponent(MeshComp);
//
//	// 카메라 리셋
//	ViewportClient.ResetCamera();
//}
//
//void UObjViewerEngine::ImportObjWithOptions(const FString& ObjPath, const FImportOptions& Options)
//{
//	UWorld* World = GetWorld();
//	if (!World) return;
//
//	// 기존 액터 모두 제거
//	TArray<AActor*> Actors = World->GetActors();
//	for (AActor* Actor : Actors)
//	{
//		World->DestroyActor(Actor);
//	}
//
//	// 옵션 기반 메시 로드 (캐시 무효화 + .bin 저장)
//	ID3D11Device* Device = Renderer.GetFD3DDevice().GetDevice();
//	UStaticMesh* Mesh = FObjManager::LoadObjStaticMesh(ObjPath, Options, Device);
//	if (!Mesh) return;
//
//	// 프리뷰 액터 생성
//	AActor* PreviewActor = World->SpawnActor<AActor>();
//	if (!PreviewActor) return;
//
//	UStaticMeshComponent* MeshComp = PreviewActor->AddComponent<UStaticMeshComponent>();
//	MeshComp->SetStaticMesh(Mesh);
//	PreviewActor->SetRootComponent(MeshComp);
//
//	// 리프레시 + 카메라 리셋
//	FObjManager::ScanObjSourceFiles();
//	ViewportClient.ResetCamera();
//}
