#pragma once

#if IS_OBJ_VIEWER

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "imgui.h"
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "Core/CoreMinimal.h"
#include "Renderer/Types/MeshPassParams.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace ViewerUI
{
    enum ECameraCommand
    {
        ECC_None = 0,
        ECC_Forward = 1,
        ECC_Back = 2,
        ECC_Up = 3,
        ECC_Bottom = 4,
        ECC_Left = 5,
        ECC_Right = 6,
    };

    struct FViewerUIInput
    {
        ERasterizerCullMode SelectedCullMode = ERasterizerCullMode::CULL_Back;
        const char*    MeshName          = nullptr;
        float          FPS               = 0.f;
        EViewModeIndex CurrentViewMode   = EViewModeIndex::VMI_Lit;
        bool           bConvertCoords    = false;  // LH Y-up → Z-up
        FVector        ModelScale        = FVector(1.f, 1.f, 1.f);
        float          AbsoluteScale     = 1.f;
    };

    struct FViewerUIOutput
    {
        ERasterizerCullMode SelectedCullMode = ERasterizerCullMode::CULL_Back;
        ECameraCommand CameraCommand     = ECC_None;
        bool           bOpenRequested    = false;
        EViewModeIndex SelectedViewMode  = EViewModeIndex::VMI_Lit;
        bool           bConvertCoords    = false;
        FVector        ModelScale        = FVector(1.f, 1.f, 1.f);
        bool           bIsScaleChanged   = false;
        float          AbsoluteScale     = 1.f;
    };

    class FViewerImGui
    {
    public:
        void Init(HWND Hwnd, ID3D11Device* Device, ID3D11DeviceContext* Context);
        void Shutdown();

        /** Forward a Windows message to ImGui. Returns the ImGui handler result. */
        LRESULT ForwardMessage(HWND HWnd, UINT Msg, WPARAM WParam, LPARAM LParam);

        /** True when ImGui wants to consume mouse input. */
        bool WantCaptureMouse() const;

        /**
         * Run a full ImGui frame: begin frame, draw both panels, render, end frame.
         * @return Output struct with button result and selected view mode.
         */
        FViewerUIOutput Draw(const FViewerUIInput& Input);

    private:
        static constexpr const char* ViewModeLabels[] = {"Lit", "Unlit", "Wireframe"};
      static constexpr const char* CullModeLabels[]   = {"None", "Cull Back", "Cull Front"};

        bool           DrawLoadPanel(const char* MeshName, float FPS);
        void           DrawControlPanel(const FViewerUIInput& Input, FViewerUIOutput& Out);
        void           DrawViewModePanel(EViewModeIndex Current, FViewerUIOutput& Out);
        void           DrawCullModePanel(ERasterizerCullMode Current, FViewerUIOutput& Out);
        void           DrawCameraPanel(FViewerUIOutput& Out);
        void           DrawScalePanel(const FViewerUIInput& Input, FViewerUIOutput& Out);
    };
} // namespace ViewerUI

#endif // IS_OBJ_VIEWER
