#include "GizmoToolbar.h"
#include "Component/GizmoComponent.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/Device/D3DDevice.h"
#include "Platform/Paths.h"
#include "Settings/EditorSettings.h"

#include <imgui.h>
#include "WICTextureLoader.h"

static const wchar_t* GetToolbarIconFileName(EToolbarIcon Icon)
{
	switch (Icon)
	{
	case EToolbarIcon::Menu: return L"Menu.png";
	case EToolbarIcon::Setting: return L"Setting.png";
	case EToolbarIcon::AddActor: return L"Add_Actor.png";
	case EToolbarIcon::Translate: return L"Translate.png";
	case EToolbarIcon::Rotate: return L"Rotate.png";
	case EToolbarIcon::Scale: return L"Scale.png";
	case EToolbarIcon::WorldSpace: return L"WorldSpace.png";
	case EToolbarIcon::LocalSpace: return L"LocalSpace.png";
	case EToolbarIcon::TranslateSnap: return L"Translate_Snap.png";
	case EToolbarIcon::RotateSnap: return L"Rotate_Snap.png";
	case EToolbarIcon::ScaleSnap: return L"Scale_Snap.png";
	case EToolbarIcon::ShowFlag: return L"Show_Flag.png";
	default: return L"";
	}
}

static ID3D11ShaderResourceView** GetToolbarIconTable()
{
	static ID3D11ShaderResourceView* ToolbarIcons[static_cast<int32>(EToolbarIcon::Count)] = {};
	return ToolbarIcons;
}

static bool bToolbarIconsLoaded = false;

static void EnsureToolbarIconsLoaded(FRenderer* Renderer)
{
	if (bToolbarIconsLoaded || !Renderer) return;

	ID3D11Device* Device = Renderer->GetFD3DDevice().GetDevice();
	ID3D11ShaderResourceView** ToolbarIcons = GetToolbarIconTable();
	const std::wstring IconDir = FPaths::Combine(FPaths::RootDir(), L"Asset/Editor/ToolIcons/");
	for (int32 i = 0; i < static_cast<int32>(EToolbarIcon::Count); ++i)
	{
		const std::wstring FilePath = IconDir + GetToolbarIconFileName(static_cast<EToolbarIcon>(i));
		DirectX::CreateWICTextureFromFile(Device, FilePath.c_str(), nullptr, &ToolbarIcons[i]);
	}

	bToolbarIconsLoaded = true;
}

static ImVec2 GetToolbarIconRenderSize(EToolbarIcon Icon, float FallbackSize, float MaxIconSize)
{
	ID3D11ShaderResourceView* IconSRV = GetToolbarIconTable()[static_cast<int32>(Icon)];
	if (!IconSRV)
	{
		return ImVec2(FallbackSize, FallbackSize);
	}

	ID3D11Resource* Resource = nullptr;
	IconSRV->GetResource(&Resource);
	if (!Resource)
	{
		return ImVec2(FallbackSize, FallbackSize);
	}

	ImVec2 IconSize(FallbackSize, FallbackSize);
	D3D11_RESOURCE_DIMENSION Dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
	Resource->GetType(&Dimension);
	if (Dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
	{
		ID3D11Texture2D* Texture2D = static_cast<ID3D11Texture2D*>(Resource);
		D3D11_TEXTURE2D_DESC Desc = {};
		Texture2D->GetDesc(&Desc);
		IconSize = ImVec2(static_cast<float>(Desc.Width), static_cast<float>(Desc.Height));
	}
	Resource->Release();

	if (IconSize.x > MaxIconSize || IconSize.y > MaxIconSize)
	{
		const float Scale = (IconSize.x > IconSize.y) ? (MaxIconSize / IconSize.x) : (MaxIconSize / IconSize.y);
		IconSize.x *= Scale;
		IconSize.y *= Scale;
	}

	return IconSize;
}

static bool DrawToolbarIconButton(const char* Id, EToolbarIcon Icon, const char* FallbackLabel, float FallbackSize, float MaxIconSize)
{
	ID3D11ShaderResourceView* IconSRV = GetToolbarIconTable()[static_cast<int32>(Icon)];
	if (!IconSRV)
	{
		return ImGui::Button(FallbackLabel);
	}

	const ImVec2 IconSize = GetToolbarIconRenderSize(Icon, FallbackSize, MaxIconSize);
	return ImGui::ImageButton(Id, reinterpret_cast<ImTextureID>(IconSRV), IconSize);
}

void FGizmoToolbar::Render(const FGizmoToolbarContext& Context)
{
	UGizmoComponent* Gizmo = Context.Gizmo;
	FGizmoToolSettings* Settings = Context.Settings;
	if (!Gizmo || !Settings) return;

	EnsureToolbarIconsLoaded(Context.Renderer);

	constexpr float ToolbarHeight = 28.0f;
	constexpr float IconSize = 16.0f;
	constexpr float ButtonPadding = (ToolbarHeight - IconSize) * 0.5f;
	constexpr float ButtonSpacing = 4.0f;
	constexpr float PlayStopButtonWidth = 24.0f;
	constexpr float GroupSpacing = 12.0f;
	constexpr float ToolbarFallbackIconSize = 14.0f;
	constexpr float ToolbarMaxIconSize = 16.0f;

	ImGui::SetCursorScreenPos(ImVec2(
		Context.ToolbarLeft + ButtonPadding + (Context.bReservePlayStopSpace ? (PlayStopButtonWidth * 2.0f) + ButtonSpacing + GroupSpacing : 0.0f),
		Context.ToolbarTop + ButtonPadding));

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.3f));

	auto DrawGizmoIcon = [&](const char* Id, EToolbarIcon Icon, EGizmoMode TargetMode, const char* FallbackLabel) -> bool
	{
		const bool bSelected = (Gizmo->GetMode() == TargetMode);
		if (bSelected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
		}
		const bool bClicked = DrawToolbarIconButton(Id, Icon, FallbackLabel, ToolbarFallbackIconSize, ToolbarMaxIconSize);
		if (bSelected)
		{
			ImGui::PopStyleColor();
		}
		return bClicked;
	};

	if (Context.bShowAddActor)
	{
		if (DrawToolbarIconButton("###SharedAddActorIcon", EToolbarIcon::AddActor, "Add Actor", ToolbarFallbackIconSize, ToolbarMaxIconSize))
		{
			if (Context.OnAddActorClicked)
			{
				Context.OnAddActorClicked();
			}
		}

		ImGui::SameLine(0, GroupSpacing);
	}

	if (DrawGizmoIcon("###TranslateIcon", EToolbarIcon::Translate, EGizmoMode::Translate, "Translate"))
	{
		Gizmo->SetTranslateMode();
	}

	ImGui::SameLine(0, ButtonSpacing);
	if (DrawGizmoIcon("###RotateIcon", EToolbarIcon::Rotate, EGizmoMode::Rotate, "Rotate"))
	{
		Gizmo->SetRotateMode();
	}

	ImGui::SameLine(0, ButtonSpacing);
	if (DrawGizmoIcon("###ScaleIcon", EToolbarIcon::Scale, EGizmoMode::Scale, "Scale"))
	{
		Gizmo->SetScaleMode();
	}

	ImGui::PopStyleColor(3);

	ImGui::SameLine(0, GroupSpacing);

	const bool bWorldCoord = Settings->CoordSystem == EEditorCoordSystem::World;
	if (bWorldCoord)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
	}

	if (DrawToolbarIconButton("###WorldSpaceIcon",
		bWorldCoord ? EToolbarIcon::WorldSpace : EToolbarIcon::LocalSpace,
		bWorldCoord ? "World" : "Local",
		ToolbarFallbackIconSize,
		ToolbarMaxIconSize))
	{
		if (Context.OnCoordSystemToggled)
		{
			Context.OnCoordSystemToggled();
		}
	}

	if (bWorldCoord)
	{
		ImGui::PopStyleColor();
	}

	auto DrawSnapControl = [&](const char* Id, EToolbarIcon Icon, const char* FallbackLabel, bool& bEnabled, float& Value, float MinValue)
	{
		ImGui::SameLine(0.0f, 6.0f);
		ImGui::PushID(Id);
		const bool bWasEnabled = bEnabled;
		if (bWasEnabled)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.38f, 0.58f, 0.88f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.42f, 0.72f, 1.0f));
		}
		if (DrawToolbarIconButton("##SnapToggle", Icon, FallbackLabel, ToolbarFallbackIconSize, ToolbarMaxIconSize))
		{
			bEnabled = !bEnabled;
		}
		if (bWasEnabled)
		{
			ImGui::PopStyleColor(3);
		}
		ImGui::SameLine(0.0f, 2.0f);
		ImGui::SetNextItemWidth(48.0f);
		if (ImGui::InputFloat("##Value", &Value, 0.0f, 0.0f, "%.2f") && Value < MinValue)
		{
			Value = MinValue;
		}
		ImGui::PopID();
	};

	if (Context.bShowSnapControls)
	{
		DrawSnapControl("TranslateSnap", EToolbarIcon::TranslateSnap, "T", Settings->bEnableTranslationSnap, Settings->TranslationSnapSize, 0.001f);
		DrawSnapControl("RotateSnap", EToolbarIcon::RotateSnap, "R", Settings->bEnableRotationSnap, Settings->RotationSnapSize, 0.001f);
		DrawSnapControl("ScaleSnap", EToolbarIcon::ScaleSnap, "S", Settings->bEnableScaleSnap, Settings->ScaleSnapSize, 0.001f);
	}

	if (Context.OnSettingsChanged)
	{
		Context.OnSettingsChanged();
	}
}
