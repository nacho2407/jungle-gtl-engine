#pragma once

#include "Core/CoreTypes.h"
#include "Editor/Viewport/FSceneViewport.h"
#include "Editor/Viewport/EditorViewportClient.h"

class UEditorEngine;
class UWorld;
class FSelectionManager;
class FWindowsWindow;
struct ID3D11ShaderResourceView;

class FEditorViewer
{
public:
    void Init(
        FWindowsWindow* InWindow,
        UEditorEngine* InEditor,
        UWorld* InWorld,
        FSelectionManager* InSelectionManager);

    void Shutdown();

    void Tick(float DeltaTime);

    void SetRect(const FViewportRect& InRect) 
    { 
        Viewport.SetRect(InRect); 
        Client.SetViewportSize((float)InRect.Width, (float)InRect.Height);
    }

	ID3D11ShaderResourceView* GetSRV() const
    {
        return Viewport.GetOutSRV();
    }

	FSceneViewport& GetViewport() { return Viewport; }

private:

private:
    FSceneViewport Viewport;
    FEditorViewportClient Client;

    UEditorEngine* Editor = nullptr;
    FWindowsWindow* Window = nullptr;
};