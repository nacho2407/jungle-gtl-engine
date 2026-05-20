#include "ImGuiSetting.h"
#include "imgui.h"
#include "Core/CoreTypes.h"
#include "Platform/Paths.h"
#include "Math/Vector.h"
#include "SimpleJSON/json.hpp"

#include <filesystem>
#include <fstream>

namespace
{
	FString GetImGuiStyleSettingsPath()
	{
		return FPaths::ToUtf8(FPaths::SettingsDir() + L"ImGuiStyle.ini");
	}
}

void ImGuiSetting::ShowSetting()
{
	if (!ImGui::Begin("ImGuiSetting"))
	{
		ImGui::End();
		return;
	}

	ImGuiStyle& Style = ImGui::GetStyle();
	ImVec4* colors = Style.Colors;

	if (ImGui::Button("Save Setting"))
		SaveSetting();

	ImGui::Separator();

	ImGui::DragFloat("WindowRounding", &Style.WindowRounding, 1.f, 0.f);
	ImGui::DragFloat("FrameRounding", &Style.FrameRounding, 1.f, 0.f);
	ImGui::DragFloat("GrabRounding", &Style.GrabRounding, 1.f, 0.f);
	ImGui::DragFloat("ScrollbarRounding", &Style.ScrollbarRounding, 1.f, 0.f);
	ImGui::DragFloat("WindowBorderSize", &Style.WindowBorderSize, 1.f, 0.f);
	ImGui::DragFloat("FrameBorderSize", &Style.FrameBorderSize, 1.f, 0.f);

	ImGui::Separator();

	const float FooterHeight = 0.0f;
	if (ImGui::BeginChild("##StyleScrollRegion", ImVec2(0, -FooterHeight), false))
	{
		for (int i = 0; i < ImGuiCol_COUNT; i++)
		{
			ImGui::ColorEdit4(ImGui::GetStyleColorName(i), (float*)&colors[i],
				ImGuiColorEditFlags_AlphaPreviewHalf);
		}
	}
	ImGui::EndChild();

	ImGui::End();
}

void ImGuiSetting::SaveSetting()
{
	using namespace json;

	ImGuiStyle& Style = ImGui::GetStyle();

	JSON Root = Object();
	JSON Colors = Array();

	for (int i = 0; i < ImGuiCol_COUNT; ++i)
	{
		const ImVec4& Color = Style.Colors[i];
		Colors.append(Array(Color.x, Color.y, Color.z, Color.w));
	}

	Root["Colors"] = Colors;
	Root["Alpha"] = Style.Alpha;
	Root["WindowRounding"] = Style.WindowRounding;
	Root["FrameRounding"] = Style.FrameRounding;
	Root["GrabRounding"] = Style.GrabRounding;
	Root["ScrollbarRounding"] = Style.ScrollbarRounding;
	Root["WindowBorderSize"] = Style.WindowBorderSize;
	Root["FrameBorderSize"] = Style.FrameBorderSize;

	const FString Path = GetImGuiStyleSettingsPath();
	std::filesystem::path FilePath(FPaths::ToWide(Path));
	if (FilePath.has_parent_path())
	{
		std::filesystem::create_directories(FilePath.parent_path());
	}

	std::ofstream File(FilePath);
	if (File.is_open())
	{
		File << Root;
	}
}

void ImGuiSetting::LoadSetting()
{
	using namespace json;

	ImGui::StyleColorsDark();

	const FString Path = GetImGuiStyleSettingsPath();
	std::ifstream File(std::filesystem::path(FPaths::ToWide(Path)));
	if (!File.is_open())
	{
		return;
	}

	FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	JSON Root = JSON::Load(Content);
	if (Root.IsNull())
	{
		return;
	}

	ImGuiStyle& Style = ImGui::GetStyle();

	if (Root.hasKey("Colors"))
	{
		JSON Colors = Root["Colors"];
		int32 Count = static_cast<int32>(Colors.length());
		if (Count > ImGuiCol_COUNT)
		{
			Count = ImGuiCol_COUNT;
		}

		for (int32 i = 0; i < Count; ++i)
		{
			JSON Color = Colors[i];
			Style.Colors[i] = ImVec4(
				static_cast<float>(Color[0].ToFloat()),
				static_cast<float>(Color[1].ToFloat()),
				static_cast<float>(Color[2].ToFloat()),
				static_cast<float>(Color[3].ToFloat()));
		}
	}

	if (Root.hasKey("Alpha")) Style.Alpha = static_cast<float>(Root["Alpha"].ToFloat());
	if (Root.hasKey("WindowRounding")) Style.WindowRounding = static_cast<float>(Root["WindowRounding"].ToFloat());
	if (Root.hasKey("FrameRounding")) Style.FrameRounding = static_cast<float>(Root["FrameRounding"].ToFloat());
	if (Root.hasKey("GrabRounding")) Style.GrabRounding = static_cast<float>(Root["GrabRounding"].ToFloat());
	if (Root.hasKey("ScrollbarRounding")) Style.ScrollbarRounding = static_cast<float>(Root["ScrollbarRounding"].ToFloat());
	if (Root.hasKey("WindowBorderSize")) Style.WindowBorderSize = static_cast<float>(Root["WindowBorderSize"].ToFloat());
	if (Root.hasKey("FrameBorderSize")) Style.FrameBorderSize = static_cast<float>(Root["FrameBorderSize"].ToFloat());
}
