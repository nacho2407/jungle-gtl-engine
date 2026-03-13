#include "SceneManager.h"
#include <fstream>
#include <iostream>

SceneManager::SceneManager()
{
	NewScene();
}

SceneManager::~SceneManager()
{
	UnloadScene();
}

FScene* SceneManager::GetActiveScene()
{
	return this->activeScene;
}

FScene* SceneManager::NewScene()
{
	// TODO: 리소스 관리자 완성되면 리소스 관리자 통하여 파일 입출력하도록 수정
	if(activeScene != nullptr)
		UnloadScene();
	activeScene = new FScene();
	return activeScene;
}

void SceneManager::SaveScene(FString sceneName)
{
	FString jsonStr = activeScene->ToJson();
	const FString filePath = sceneName + ".scene";
	std::ofstream outFile(filePath);

	if (outFile.is_open()) 
	{
		outFile << jsonStr;
		outFile.close();
		std::cout << "Saved scene file: " << filePath << std::endl;
	}
	else 
	{
		// TODO: 디버깅 에러 출력 변경
		std::cerr << "Cannot open file: " << filePath << std::endl;
	}
	return;
}


FScene* SceneManager::LoadScene(FString sceneName)
{
	// TODO: 리소스 관리자 완성되면 리소스 관리자 통하여 파일 입출력하도록 수정
	const FString filePath = sceneName + ".scene";
	std::ifstream inFile;
	inFile.open(filePath);
	if (inFile.is_open())
	{
		FString jsonStr;
		inFile >> jsonStr;
		return FScene::FromJson(jsonStr);
	}
	else
	{
		// TODO: 디버깅 에러 출력 변경
		std::cerr << "Cannot open file: " << filePath << std::endl;
	}
	return new FScene();
}

void SceneManager::UnloadScene()
{
	delete activeScene;
	activeScene = nullptr;
}


