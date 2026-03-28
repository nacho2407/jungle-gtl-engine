#pragma once
#include "CoreMinimal.h"
#include "Scene/SceneTypes.h"
#include "Windows.h"
#include "Core/Core.h"
#include "ViewportClient.h"
#include <memory>

class CWindowApplication;
class CWindow;

class ENGINE_API FEngine
{
public:
	FEngine() = default;
	virtual ~FEngine();

	FEngine(const FEngine&) = delete;
	FEngine& operator=(const FEngine&) = delete;

	bool Initialize(HINSTANCE hInstance, const wchar_t* Title, int32 Width, int32 Height);
	void Run();
	virtual void Shutdown();

	CCore* GetCore() const { return Core.get(); }
	CWindowApplication* GetApp() const { return App; }

protected:
	virtual void PreInitialize() {}
	virtual void PostInitialize() {}
	virtual void Tick(float DeltaTime) {}
	virtual ESceneType GetStartupSceneType() const { return ESceneType::Game; }
	virtual std::unique_ptr<IViewportClient> CreateViewportClient();

	CWindowApplication* App = nullptr;
	CWindow* MainWindow = nullptr;
	std::unique_ptr<CCore> Core;
	std::unique_ptr<IViewportClient> ViewportClient;

private:
	bool OnInput(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	void OnResize(int32 Width, int32 Height);
};
