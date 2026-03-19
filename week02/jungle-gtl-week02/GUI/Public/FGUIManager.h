#pragma once

class FWindowsApplication;
class URenderer;

class FGUIManager
{
public:
    FGUIManager();
    ~FGUIManager();

public:
    bool Initialize(FWindowsApplication* WindowApp, URenderer* Renderer);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    bool WantsMouseCapture() const;
    bool WantsKeyboardCapture() const;

private:
    FWindowsApplication* WindowApp = nullptr;
    URenderer* Renderer = nullptr;
    bool bInitialized = false;
};