#include "Editor/UI/EditorMainPanel.h"

#include "Engine/Core/Paths.h"

#include "WICTextureLoader.h"

namespace
{
const wchar_t* GetViewportToolIconFileName(EEditorMainPanelViewportToolIcon Icon)
{
    switch (Icon)
    {
    case EEditorMainPanelViewportToolIcon::Menu: return L"Menu.png";
    case EEditorMainPanelViewportToolIcon::Select: return L"Select.png";
    case EEditorMainPanelViewportToolIcon::Translate: return L"Translate.png";
    case EEditorMainPanelViewportToolIcon::Rotate: return L"Rotate.png";
    case EEditorMainPanelViewportToolIcon::Scale: return L"Scale.png";
    case EEditorMainPanelViewportToolIcon::TranslateSnap: return L"Translate_Snap.png";
    case EEditorMainPanelViewportToolIcon::RotateSnap: return L"Rotate_Snap.png";
    case EEditorMainPanelViewportToolIcon::ScaleSnap: return L"Scale_Snap.png";
    case EEditorMainPanelViewportToolIcon::WorldSpace: return L"WorldSpace.png";
    case EEditorMainPanelViewportToolIcon::LocalSpace: return L"LocalSpace.png";
    case EEditorMainPanelViewportToolIcon::Camera: return L"Camera.png";
    case EEditorMainPanelViewportToolIcon::Setting: return L"Setting.png";
    default: return L"";
    }
}

const wchar_t* GetViewportLayoutIconFileName(EEditorViewportLayoutMode Mode)
{
    switch (Mode)
    {
    case EEditorViewportLayoutMode::OnePane: return L"ViewportLayout_OnePane.png";
    case EEditorViewportLayoutMode::TwoPanesHoriz: return L"ViewportLayout_TwoPanesHoriz.png";
    case EEditorViewportLayoutMode::TwoPanesVert: return L"ViewportLayout_TwoPanesVert.png";
    case EEditorViewportLayoutMode::ThreePanesLeft: return L"ViewportLayout_ThreePanesLeft.png";
    case EEditorViewportLayoutMode::ThreePanesRight: return L"ViewportLayout_ThreePanesRight.png";
    case EEditorViewportLayoutMode::ThreePanesTop: return L"ViewportLayout_ThreePanesTop.png";
    case EEditorViewportLayoutMode::ThreePanesBottom: return L"ViewportLayout_ThreePanesBottom.png";
    case EEditorViewportLayoutMode::FourPanes2x2: return L"ViewportLayout_FourPanes2x2.png";
    case EEditorViewportLayoutMode::FourPanesLeft: return L"ViewportLayout_FourPanesLeft.png";
    case EEditorViewportLayoutMode::FourPanesRight: return L"ViewportLayout_FourPanesRight.png";
    case EEditorViewportLayoutMode::FourPanesTop: return L"ViewportLayout_FourPanesTop.png";
    case EEditorViewportLayoutMode::FourPanesBottom: return L"ViewportLayout_FourPanesBottom.png";
    default: return L"";
    }
}
} // namespace

void FEditorMainPanel::LoadViewportToolIcons(ID3D11Device* Device)
{
    if (!Device)
    {
        return;
    }

    const std::wstring IconDir = FPaths::Combine(FPaths::RootDir(), L"Asset/Editor/ToolIcons/");
    for (int32 i = 0; i < static_cast<int32>(EEditorMainPanelViewportToolIcon::Count); ++i)
    {
        ID3D11ShaderResourceView*& SRV = IconResources.ToolIcons[i];
        if (SRV)
        {
            continue;
        }

        const std::wstring IconPath =
            IconDir + GetViewportToolIconFileName(static_cast<EEditorMainPanelViewportToolIcon>(i));
        DirectX::CreateWICTextureFromFile(Device, IconPath.c_str(), nullptr, &SRV);
    }

    if (!IconResources.AddActorIcon)
    {
        const std::wstring AddActorIconPath = IconDir + L"Add_Actor.png";
        DirectX::CreateWICTextureFromFile(Device, AddActorIconPath.c_str(), nullptr, &IconResources.AddActorIcon);
    }
    if (!IconResources.SaveIcon)
    {
        const std::wstring SaveIconPath = IconDir + L"Save.png";
        DirectX::CreateWICTextureFromFile(Device, SaveIconPath.c_str(), nullptr, &IconResources.SaveIcon);
    }
    if (!IconResources.HotReloadIcon)
    {
        const std::wstring HotReloadIconPath = IconDir + L"HotReload.png";
        DirectX::CreateWICTextureFromFile(Device, HotReloadIconPath.c_str(), nullptr, &IconResources.HotReloadIcon);
    }

    const std::wstring LayoutIconDir = FPaths::Combine(FPaths::RootDir(), L"Asset/Editor/Icons/");
    for (int32 i = 0; i < static_cast<int32>(EEditorViewportLayoutMode::Max); ++i)
    {
        ID3D11ShaderResourceView*& SRV = IconResources.LayoutIcons[i];
        if (SRV)
        {
            continue;
        }

        const std::wstring IconPath = LayoutIconDir + GetViewportLayoutIconFileName(static_cast<EEditorViewportLayoutMode>(i));
        DirectX::CreateWICTextureFromFile(Device, IconPath.c_str(), nullptr, &SRV);
    }
}

void FEditorMainPanel::ReleaseViewportToolIcons()
{
    for (int32 i = 0; i < static_cast<int32>(EEditorMainPanelViewportToolIcon::Count); ++i)
    {
        if (IconResources.ToolIcons[i])
        {
            IconResources.ToolIcons[i]->Release();
            IconResources.ToolIcons[i] = nullptr;
        }
    }
    for (int32 i = 0; i < static_cast<int32>(EEditorViewportLayoutMode::Max); ++i)
    {
        if (IconResources.LayoutIcons[i])
        {
            IconResources.LayoutIcons[i]->Release();
            IconResources.LayoutIcons[i] = nullptr;
        }
    }
    if (IconResources.AddActorIcon)
    {
        IconResources.AddActorIcon->Release();
        IconResources.AddActorIcon = nullptr;
    }
    if (IconResources.SaveIcon)
    {
        IconResources.SaveIcon->Release();
        IconResources.SaveIcon = nullptr;
    }
    if (IconResources.HotReloadIcon)
    {
        IconResources.HotReloadIcon->Release();
        IconResources.HotReloadIcon = nullptr;
    }
}
