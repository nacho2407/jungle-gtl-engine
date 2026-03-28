#pragma once

#include "CoreMinimal.h"
#include "Windows.h"
#include <functional>
#include <memory>

class FEngine;

struct FEngineLaunchConfig
{
	const wchar_t* Title = L"Jungle";
	int32 Width = 1280;
	int32 Height = 720;
	std::function<std::unique_ptr<FEngine>()> CreateEngine;
};

/// <summary>
/// COM 초기화와 최상위 실행 시작/종료를 담당.
/// </summary>
class ENGINE_API FWindowsEngineLaunch
{
public:
	int Run(HINSTANCE hInstance, const FEngineLaunchConfig& Config);

private:
	bool InitializeProcess();
	void Shutdown();

private:
	HRESULT ComResult = E_FAIL;
};
