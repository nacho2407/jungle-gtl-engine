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
	SceneMgr->UnloadScene();
	delete SceneMgr; SceneMgr = nullptr;
	FLevel::GetInstance().Clear();
}

SceneManager* UWorld::GetSceneManager()
{
	return SceneMgr;
}

void UWorld::Update(float deltaTime)
{
	SceneMgr->GetPermanentScene()->Update(deltaTime);
	// 향후 확작성을 위한 TODO: 멀티 씬 구조에서 for each scene 루프 중첩 추가
	SceneMgr->GetActiveScene()->Update(deltaTime);
}

void UWorld::OnBeforeRender() const
{
	// 향후 확작성을 위한 TODO: 멀티 씬 구조에서 for each scene 루프 중첩 추가
	auto sceneComponents = SceneMgr->GetActiveScene()->SceneComponents;
	for (USceneComponent* s : sceneComponents)
	{
		UPrimitiveComponent* primitive = Cast<UPrimitiveComponent>(s);
		if (primitive != nullptr)
		{
			if (primitive->GetRenderObject()->bIsDirty)
				primitive->ResolveRenderStateDirty();
		}
	}
}

UWorld& GetWorld()
{
	return UWorld::GetInstance();
}