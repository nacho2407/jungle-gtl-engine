#include "World.h"
#include "EngineCore/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "EngineCore/EngineStatics.h"
#include "SerializeHelper.h"

//UScene* UWorld::activeScene;
//UScene* UWorld::permanentScene;

UWorld::UWorld()
{
	SceneMgr = new SceneManager();
	//permanentScene = Cast<UScene>(FObjectFactory::ConstructObject(UScene::GetClass()));
}

UWorld::~UWorld() {
	while(!SceneMgr->GetLoadedScenes().empty())
	{
		UScene* scene = SceneMgr->GetLoadedScenes().back();
		SceneMgr->UnloadScene(scene);
	}
	delete SceneMgr;
	SceneMgr = nullptr;
}

SceneManager* UWorld::GetSceneManager()
{
	return SceneMgr;
}

void UWorld::Update(float deltaTime)
{
	if (SceneMgr->GetPermanentScene())
		SceneMgr->GetPermanentScene()->Update(deltaTime);
	// 향후 확작성을 위한 TODO: 멀티 씬 구조에서 for each scene 루프 중첩 추가
	if (SceneMgr->GetActiveScene())
		SceneMgr->GetActiveScene()->Update(deltaTime);
}

void UWorld::OnBeforeRender() const
{
	// 향후 확작성을 위한 TODO: 멀티 씬 구조에서 for each scene 루프 중첩 추가
	UScene* activeScene = SceneMgr->GetActiveScene();
	UScene* permanentScene = SceneMgr->GetPermanentScene();

	auto UpdateScene = [](UScene* scene) {
		if (scene == nullptr) return;
		auto sceneComponents = scene->SceneComponents;
		for (USceneComponent* s : sceneComponents)
		{
			UPrimitiveComponent* primitive = Cast<UPrimitiveComponent>(s);
			if (primitive != nullptr)
			{
				if (primitive->GetRenderObject() && primitive->GetRenderObject()->bIsDirty)
					primitive->ResolveRenderStateDirty();
			}
		}
	};

	UpdateScene(activeScene);
	UpdateScene(permanentScene);
}

UWorld& GetWorld()
{
	return UWorld::GetInstance();
}