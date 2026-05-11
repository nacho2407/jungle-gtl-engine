#include "ViewportToolbar.h"
#include "Component/GizmoComponent.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/Device/D3DDevice.h"
#include "Platform/Paths.h"
#include "Settings/EditorViewportSettings.h"
#include "Settings/GizmoToolSettings.h"

#include <imgui.h>
#include "WICTextureLoader.h"

#pragma region Toolbar Icon Helper
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
	case EToolbarIcon::Camera: return L"Camera.png";
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
#pragma endregion

#pragma region LeftRight Section Helper

static float CalcRightToolbarWidth(const FViewportToolbarContext& Context)
{
	constexpr float IconButtonWidth = 24.0f;
	constexpr float TextButtonMinWidth = 90.0f;
	constexpr float ButtonSpacing = 4.0f;
	constexpr float GroupSpacing = 12.0f;

	float Width = 0.0f;
	
	auto AddGroup = [&](float ItemWidth)
	{
		if (Width > 0.0f)
		{
			Width += GroupSpacing;
		}
		Width += ItemWidth;
	};

	if (Context.bShowViewportType)
	{
		AddGroup(TextButtonMinWidth);
	}

	if (Context.bShowCameraControls)
	{
		AddGroup(IconButtonWidth);
	}

	if (Context.bShowViewMode)
	{
		AddGroup(TextButtonMinWidth);
	}

	if (Context.bShowShowFlags)
	{
		AddGroup(IconButtonWidth);
	}

	if (Context.bShowLayoutControls)
	{
		AddGroup(IconButtonWidth * 2 + ButtonSpacing);
	}

	return Width;
}

static void SameLineIfNeeded(bool& bHasItem, float Spacing)
{
	if (bHasItem)
	{
		ImGui::SameLine(0, Spacing);
	}
	bHasItem = true;
}

#pragma endregion

void FViewportToolbar::Render(const FViewportToolbarContext& Context)
{
	if (!Context.Settings) return;

	EnsureToolbarIconsLoaded(Context.Renderer);

	FToolbarRenderState RenderState(Context);

	ImGui::PushID(static_cast<int32>(Context.ToolbarLeft));
	ImGui::PushID(static_cast<int32>(Context.ToolbarTop));

	BeginToolbar(RenderState);

	RenderLeftToolbarSection(RenderState);

	const float RightWidth = CalcRightToolbarWidth(Context);
	const float RightX = Context.ToolbarLeft + Context.ToolbarWidth - RightWidth - 8.0f;

	ImGui::SetCursorScreenPos(ImVec2(RightX, Context.ToolbarTop + 6.0f));

	RenderRightToolbarSection(RenderState);

	EndToolbar(RenderState);

	ImGui::PopID();
	ImGui::PopID();

	if (Context.OnSettingsChanged)
	{
		Context.OnSettingsChanged();
	}
}

void FViewportToolbar::BeginToolbar(const FToolbarRenderState& State)
{
	constexpr float ToolbarHeight = 28.0f;
	constexpr float IconSize = 16.0f;
	constexpr float ButtonPadding = (ToolbarHeight - IconSize) * 0.5f;
	constexpr float PlayStopButtonWidth = 24.0f;

	const float PlayStopOffset = State.Context.bReservePlayStopSpace ? (PlayStopButtonWidth * 2.0f) + ButtonPadding : 0.0f;

	ImGui::SetCursorScreenPos(ImVec2(
		State.Context.ToolbarLeft + ButtonPadding + PlayStopOffset,
		State.Context.ToolbarTop + ButtonPadding));

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.3f));
}

void FViewportToolbar::EndToolbar(const FToolbarRenderState& State)
{
	ImGui::PopStyleColor(3);
}

void FViewportToolbar::RenderLeftToolbarSection(const FToolbarRenderState& State)
{
	if (State.Context.bShowAddActor)
	{
		RenderAddActor(State);
	}

	if (State.Context.bShowGizmoControls)
	{
		RenderGizmoControls(State);
	}
}

void FViewportToolbar::RenderRightToolbarSection(const FToolbarRenderState& State)
{
	bool bHasItem = false;

	if (State.Context.bShowViewportType)
	{
		SameLineIfNeeded(bHasItem, State.GroupSpacing);
		RenderViewportType(State);
	}

	if (State.Context.bShowCameraControls)
	{
		SameLineIfNeeded(bHasItem, State.GroupSpacing);
		RenderCameraControls(State);
	}

	if (State.Context.bShowViewMode)
	{
		SameLineIfNeeded(bHasItem, State.GroupSpacing);
		RenderViewMode(State);
	}

	if (State.Context.bShowShowFlags)
	{
		SameLineIfNeeded(bHasItem, State.GroupSpacing);
		RenderShowFlags(State);
	}

	if (State.Context.bShowLayoutControls)
	{
		SameLineIfNeeded(bHasItem, State.GroupSpacing);
		RenderLayoutControls(State);
	}
}

void FViewportToolbar::RenderLayoutControls(const FToolbarRenderState& State)
{
	if (DrawToolbarIconButton("##Layout", EToolbarIcon::Menu, "Layout",
		State.FallbackIconSize, State.MaxIconSize))
	{
		ImGui::OpenPopup("##LayoutPopup");
	}

	if (ImGui::BeginPopup("##LayoutPopup"))
	{
		constexpr int32 Columns = 4;
		constexpr float IconSize = 32.0f;
		const int32 LayoutCount = State.Context.LayoutIconCount;
		for (int32 LayoutIndex = 0; LayoutIndex < LayoutCount; ++LayoutIndex)
		{
			ImGui::PushID(LayoutIndex);

			const bool bSelected = LayoutIndex == State.Context.CurrentLayoutIndex;
			if (bSelected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
			}

			bool bClicked = false;
			ID3D11ShaderResourceView* LayoutIcon =
				State.Context.LayoutIcons ? State.Context.LayoutIcons[LayoutIndex] : nullptr;
			if (LayoutIcon)
			{
				bClicked = ImGui::ImageButton("##LayoutIcon",
					reinterpret_cast<ImTextureID>(LayoutIcon),
					ImVec2(IconSize, IconSize));
			}
			else
			{
				char FallbackLabel[8];
				snprintf(FallbackLabel, sizeof(FallbackLabel), "%d", LayoutIndex);
				bClicked = ImGui::Button(FallbackLabel, ImVec2(IconSize + 8.0f, IconSize + 8.0f));
			}

			if (bSelected)
			{
				ImGui::PopStyleColor();
			}

			if (bClicked)
			{
				if (State.Context.OnLayoutSelected)
				{
					State.Context.OnLayoutSelected(LayoutIndex);
				}
				ImGui::CloseCurrentPopup();
			}

			if ((LayoutIndex + 1) % Columns != 0 && LayoutIndex + 1 < LayoutCount)
			{
				ImGui::SameLine();
			}

			ImGui::PopID();
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine(0.0f, State.ButtonSpacing);

	bool bToggleClicked = false;
	ID3D11ShaderResourceView* ToggleIcon = nullptr;
	if (State.Context.LayoutIcons &&
		State.Context.ToggleLayoutIndex >= 0 &&
		State.Context.ToggleLayoutIndex < State.Context.LayoutIconCount)
	{
		ToggleIcon = State.Context.LayoutIcons[State.Context.ToggleLayoutIndex];
	}

	if (ToggleIcon)
	{
		bToggleClicked = ImGui::ImageButton("##ToggleLayout",
			reinterpret_cast<ImTextureID>(ToggleIcon),
			ImVec2(State.MaxIconSize, State.MaxIconSize));
	}
	else
	{
		bToggleClicked = DrawToolbarIconButton("##ToggleLayout", EToolbarIcon::Menu, "Toggle",
			State.FallbackIconSize, State.MaxIconSize);
	}

	if (bToggleClicked)
	{
		if (State.Context.OnToggleLayout)
		{
			State.Context.OnToggleLayout();
		}
	}
}

void FViewportToolbar::RenderAddActor(const FToolbarRenderState& State)
{
	if (DrawToolbarIconButton("###SharedAddActorIcon", EToolbarIcon::AddActor, "Add Actor",
		State.FallbackIconSize, State.MaxIconSize))
	{
		if (State.Context.OnAddActorClicked)
		{
			State.Context.OnAddActorClicked();
		}
	}

	ImGui::SameLine(0, State.GroupSpacing);
}

void FViewportToolbar::RenderViewportType(const FToolbarRenderState& State)
{
	FViewportRenderOptions& RenderOptions = State.RenderOptions();

	static const char* ViewportTypeNames[] = {
		"Perspective",
		"Top",
		"Bottom",
		"Left",
		"Right",
		"Front",
		"Back",
		"Free Orthographic"
	};

	constexpr int32 ViewportTypeCount = sizeof(ViewportTypeNames) / sizeof(ViewportTypeNames[0]);
	int32 CurrentTypeIndex = static_cast<int32>(RenderOptions.ViewportType);
	if (CurrentTypeIndex < 0 || CurrentTypeIndex >= ViewportTypeCount)
	{
		CurrentTypeIndex = 0;
	}

	ImGui::SetNextItemWidth(90.0f);
	if (ImGui::Button(ViewportTypeNames[CurrentTypeIndex], ImVec2(90.0f, 0.0f)))
	{
		ImGui::OpenPopup("##ViewportTypePopup");
	}

	if (ImGui::BeginPopup("##ViewportTypePopup"))
	{
		for (int32 TypeIndex = 0; TypeIndex < ViewportTypeCount; ++TypeIndex)
		{
			const bool bSelected = TypeIndex == CurrentTypeIndex;
			if (ImGui::Selectable(ViewportTypeNames[TypeIndex], bSelected))
			{
				const ELevelViewportType NewType = static_cast<ELevelViewportType>(TypeIndex);
				if (State.Context.OnViewportTypeSelected)
				{
					State.Context.OnViewportTypeSelected(NewType);
				}
				else
				{
					RenderOptions.ViewportType = NewType;
				}
			}
		}
		ImGui::EndPopup();
	}
}

void FViewportToolbar::RenderGizmoControls(const FToolbarRenderState& State)
{
	UGizmoComponent* Gizmo = State.Context.Gizmo;
	if (!Gizmo) return;

	auto DrawGizmoIcon = [&](const char* Id, EToolbarIcon Icon, EGizmoMode TargetMode, const char* FallbackLabel) -> bool
	{
		const bool bSelected = (Gizmo->GetMode() == TargetMode);
		if (bSelected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
		}
		const bool bClicked = DrawToolbarIconButton(Id, Icon, FallbackLabel, State.FallbackIconSize, State.MaxIconSize);
		if (bSelected)
		{
			ImGui::PopStyleColor();
		}
		return bClicked;
	};

	if (DrawGizmoIcon("###TranslateIcon", EToolbarIcon::Translate, EGizmoMode::Translate, "Translate"))
	{
		Gizmo->SetTranslateMode();
	}

	ImGui::SameLine(0, State.ButtonSpacing);
	if (DrawGizmoIcon("###RotateIcon", EToolbarIcon::Rotate, EGizmoMode::Rotate, "Rotate"))
	{
		Gizmo->SetRotateMode();
	}

	ImGui::SameLine(0, State.ButtonSpacing);
	if (DrawGizmoIcon("###ScaleIcon", EToolbarIcon::Scale, EGizmoMode::Scale, "Scale"))
	{
		Gizmo->SetScaleMode();
	}

	ImGui::SameLine(0, State.GroupSpacing);

	RenderCoordSystemButton(State);
	RenderSnapControls(State);
}

void FViewportToolbar::RenderCoordSystemButton(const FToolbarRenderState& State)
{
	FGizmoToolSettings& Settings = State.GizmoSettings();

	const bool bWorldCoord = Settings.CoordSystem == EEditorCoordSystem::World;
	if (bWorldCoord)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
	}

	if (DrawToolbarIconButton("###WorldSpaceIcon",
		bWorldCoord ? EToolbarIcon::WorldSpace : EToolbarIcon::LocalSpace,
		bWorldCoord ? "World" : "Local",
		State.FallbackIconSize,
		State.MaxIconSize))
	{
		if (State.Context.OnCoordSystemToggled)
		{
			State.Context.OnCoordSystemToggled();
		}
	}

	if (bWorldCoord)
	{
		ImGui::PopStyleColor();
	}
}

void FViewportToolbar::RenderSnapControls(const FToolbarRenderState& State)
{
	FGizmoToolSettings& Settings = State.GizmoSettings();

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
		if (DrawToolbarIconButton("##SnapToggle", Icon, FallbackLabel, State.FallbackIconSize, State.MaxIconSize))
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

	DrawSnapControl("TranslateSnap", EToolbarIcon::TranslateSnap, "T", Settings.bEnableTranslationSnap, Settings.TranslationSnapSize, 0.001f);
	DrawSnapControl("RotateSnap", EToolbarIcon::RotateSnap, "R", Settings.bEnableRotationSnap, Settings.RotationSnapSize, 0.001f);
	DrawSnapControl("ScaleSnap", EToolbarIcon::ScaleSnap, "S", Settings.bEnableScaleSnap, Settings.ScaleSnapSize, 0.001f);
}

void FViewportToolbar::RenderCameraControls(const FToolbarRenderState& State)
{
	if (DrawToolbarIconButton("##CameraSettings", EToolbarIcon::Camera, "Camera",
		State.FallbackIconSize, State.MaxIconSize))
	{
		ImGui::OpenPopup("##CameraPopup");
	}

	if (ImGui::BeginPopup("##CameraPopup"))
	{
		auto& Camera = State.CameraSettings();

		ImGui::Text("Camera");
		ImGui::SetNextItemWidth(140.0f);
		ImGui::SliderFloat("Move", &Camera.MoveSpeed, 0.1f, 100.0f, "%.1f");

		ImGui::SetNextItemWidth(140.0f);
		ImGui::SliderFloat("Rotation", &Camera.RotationSpeed, 1.0f, 360.0f, "%.1f");

		ImGui::SetNextItemWidth(140.0f);
		ImGui::SliderFloat("Zoom", &Camera.ZoomSpeed, 10.0f, 1000.0f, "%.1f");

		ImGui::EndPopup();
	}
}

void FViewportToolbar::RenderViewMode(const FToolbarRenderState& State)
{
	FViewportRenderOptions& RenderOptions = State.RenderOptions();

	if (DrawToolbarIconButton("##ViewMode", EToolbarIcon::Setting, "View Mode",
		State.FallbackIconSize, State.MaxIconSize))
	{
		ImGui::OpenPopup("##ViewModePopup");
	}

	if (ImGui::BeginPopup("##ViewModePopup"))
	{
		int32 CurrentMode = static_cast<int32>(RenderOptions.ViewMode);

		ImGui::RadioButton("Unlit", &CurrentMode, static_cast<int32>(EViewMode::Unlit));
		ImGui::RadioButton("Phong", &CurrentMode, static_cast<int32>(EViewMode::Lit_Phong));
		ImGui::RadioButton("Gouraud", &CurrentMode, static_cast<int32>(EViewMode::Lit_Gouraud));
		ImGui::RadioButton("Lambert", &CurrentMode, static_cast<int32>(EViewMode::Lit_Lambert));
		ImGui::RadioButton("Wireframe", &CurrentMode, static_cast<int32>(EViewMode::Wireframe));
		ImGui::RadioButton("Scene Depth", &CurrentMode, static_cast<int32>(EViewMode::SceneDepth));
		ImGui::RadioButton("World Normal", &CurrentMode, static_cast<int32>(EViewMode::WorldNormal));
		ImGui::RadioButton("Light Culling", &CurrentMode, static_cast<int32>(EViewMode::LightCulling));

		RenderOptions.ViewMode = static_cast<EViewMode>(CurrentMode);
		ImGui::EndPopup();
	}
}

void FViewportToolbar::RenderShowFlags(const FToolbarRenderState& State)
{
	FViewportRenderOptions& RenderOptions = State.RenderOptions();

	if (DrawToolbarIconButton("##ShowFlags", EToolbarIcon::ShowFlag, "Show Flags",
		State.FallbackIconSize, State.MaxIconSize))
	{
		ImGui::OpenPopup("##ShowFlagsPopup");
	}

	if (ImGui::BeginPopup("##ShowFlagsPopup"))
	{
		ImGui::Text("Show Flags");

		ImGui::Checkbox("Primitives", &RenderOptions.ShowFlags.bPrimitives);
		ImGui::Checkbox("Grid", &RenderOptions.ShowFlags.bGrid);
		ImGui::Checkbox("World Axis", &RenderOptions.ShowFlags.bWorldAxis);
		ImGui::Checkbox("Gizmo", &RenderOptions.ShowFlags.bGizmo);
		ImGui::Checkbox("Billboard Text", &RenderOptions.ShowFlags.bBillboardText);
		ImGui::Checkbox("Bounding Volume", &RenderOptions.ShowFlags.bBoundingVolume);
		ImGui::Checkbox("Debug Draw", &RenderOptions.ShowFlags.bDebugDraw);
		ImGui::Checkbox("Octree", &RenderOptions.ShowFlags.bOctree);
		ImGui::Checkbox("Fog", &RenderOptions.ShowFlags.bFog);
		ImGui::Checkbox("FXAA", &RenderOptions.ShowFlags.bFXAA);
		ImGui::Checkbox("Gamma Correction", &RenderOptions.ShowFlags.bGammaCorrection);
		ImGui::Checkbox("View Light Culling", &RenderOptions.ShowFlags.bViewLightCulling);
		ImGui::Checkbox("Visualize 2.5D Culling", &RenderOptions.ShowFlags.bVisualize25DCulling);
		ImGui::Checkbox("Show Shadow Frustum", &RenderOptions.ShowFlags.bShowShadowFrustum);
		ImGui::Checkbox("Collision", &RenderOptions.ShowFlags.bCollision);
		ImGui::Checkbox("Show Collision Shape", &RenderOptions.ShowFlags.bShowCollisionShape);

		ImGui::EndPopup();
	}
}
