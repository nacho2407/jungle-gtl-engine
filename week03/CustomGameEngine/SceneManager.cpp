#include "SceneManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include "World.h"
#include "UEngineStatics.h"
#include "UPrimitiveComponent.h"
#include "Logger.h"

SceneManager::SceneManager()
{	
}

SceneManager::~SceneManager()
{
	UnloadScene();
}

UScene* SceneManager::NewScene()
{
	// TODO: 리소스 관리자 완성되면 리소스 관리자 통하여 파일 입출력하도록 수정
	if (GetWorld().GetActiveScene() != nullptr)
		UnloadScene();
	UScene* newScene = Cast<UScene>(FObjectFactory::ConstructObject(UScene::GetClass()));
	GetWorld().SetActiveScene(newScene);
	UEngineStatics::NextUUID = 0;
	UE_LOG("Created new scene\n");
	return GetWorld().GetActiveScene();
}

void SceneManager::SaveScene(FString sceneName)
{
	UScene *activeScene = GetWorld().GetActiveScene();
	activeScene->NextUUID = UEngineStatics::NextUUID;

	FString jsonStr = activeScene->ToJson();
	const FString filePath = sceneName + ".scene";
	std::ofstream outFile(filePath);

	if (outFile.is_open())
	{
		outFile << jsonStr;
		outFile.close();
		UE_LOG("Saved scene file: %s\n", filePath.c_str());
	}
	else
	{
		// TODO: 에러 출력 로그 레벨 변경
		UE_LOG("Cannot open file : %s\n", filePath.c_str());
	}
	return;
}


UScene* SceneManager::LoadScene(FString sceneName)
{
	if (GetWorld().GetActiveScene() != nullptr)
		UnloadScene();

	// TODO: 리소스 관리자 완성되면 리소스 관리자 통하여 파일 입출력하도록 수정
	const FString filePath = sceneName + ".scene";
	std::ifstream inFile;
	inFile.open(filePath);
	if (inFile.is_open())
	{
		FString jsonStr((std::istreambuf_iterator<char>(inFile)),
			std::istreambuf_iterator<char>());
		inFile >> jsonStr;
		inFile.close();

		UScene *loadedScene = UScene::FromJson(jsonStr);
		GetWorld().SetActiveScene(loadedScene);
		UEngineStatics::NextUUID = loadedScene->NextUUID;

		UE_LOG("Loaded scene file: %s\n", filePath.c_str());
		return loadedScene;
	}
	else
	{
		// TODO: 에러 출력 로그 레벨 변경
		UE_LOG("Cannot open file : %s\n", filePath.c_str());
		return new UScene();
	} 
}

void SceneManager::UnloadScene()
{
	UScene *curScene = GetWorld().GetActiveScene();

	// loop 내에서의 삽입/삭제로 인한 오류 방지
	auto deleteList = curScene->SceneComponents;
	for (auto obj : deleteList)
	{
		GetWorld().RemoveSceneComponent(obj);
	}
	delete GetWorld().GetActiveScene();
	GetWorld().SetActiveScene(nullptr);
}


