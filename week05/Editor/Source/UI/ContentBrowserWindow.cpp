#include "ContentBrowserWindow.h"
#include <filesystem>
#include <d3d11.h>
#include "Debug/EngineLog.h"
#include "Core/Paths.h"

FContentBrowserWindow::FContentBrowserWindow():
	RootPath(std::filesystem::current_path()),
	CurrentPath(std::filesystem::current_path())
{
	while (!std::filesystem::exists(RootPath / "Engine"))
	{
		RootPath = RootPath.parent_path();
	}

	RootPath = RootPath / "Assets";
	CurrentPath = RootPath;
}

void FContentBrowserWindow::Render()
{
	if (!ImGui::Begin("Content Browser"))
	{
		ImGui::End();
		return;
	}

	bIsMouseOnDirectory = false;
	bIsMouseOnFile = false;
	DirectoryPathUnderMouse = "";
	FilePathUnderMouse = "";

	// 상단 경로 + 뒤로가기
	if (ImGui::Button("<-"))
	{
		if (CurrentPath != RootPath)
		{
			CurrentPath = CurrentPath.parent_path();
		}
	}

	ImGui::SameLine();
	ImGui::Text("%s", CurrentPath.u8string().c_str());

	ImGui::Separator();

	// 좌측 폴더 트리
	ImGui::BeginChild("LeftPanel", ImVec2(200, 0), true);
	DrawFolderTree(RootPath);
	ImGui::EndChild();

	ImGui::SameLine();

	// 우측 파일 그리드
	ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
	DrawFileGrid();
	ImGui::EndChild();

	bIsHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

	ImGui::End();


	if (bFileOnDrag)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			ImVec2 MousePos = ImGui::GetMousePos();

			ImGui::SetNextWindowPos(MousePos, ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.0f);

			ImGui::Begin("DragPreview", nullptr,
				ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_NoInputs |
				ImGuiWindowFlags_AlwaysAutoResize);

			ImGui::Image(FileIcon, ImVec2(48, 48));

			ImGui::End();
		}
		else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			bFileOnDrag = false;
			OnFileDragEnd(SelectedFilePath.string(), DirectoryPathUnderMouse.string());
		}
	}
}

void FContentBrowserWindow::SetFolderIcon(ID3D11ShaderResourceView* FolderSRV)
{
	FolderIcon = (ImTextureID)(FolderSRV);
}

void FContentBrowserWindow::SetFileIcon(ID3D11ShaderResourceView* FileSRV)
{
	FileIcon = (ImTextureID)(FileSRV);
}

void FContentBrowserWindow::DrawFolderTree(const std::filesystem::path& Path)
{
	for (auto& Entry : std::filesystem::directory_iterator(Path))
	{
		if (!Entry.is_directory())
			continue;

		const auto& DirPath = Entry.path();
		auto NameUtf8 = DirPath.filename().u8string();
		std::string Name(NameUtf8.begin(), NameUtf8.end());

		ImGuiTreeNodeFlags Flags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			((CurrentPath == DirPath) ? ImGuiTreeNodeFlags_Selected : 0);

		bool bOpened = ImGui::TreeNodeEx(Name.c_str(), Flags);

		if (ImGui::IsItemClicked())
		{
			CurrentPath = DirPath;
		}

		if (bOpened)
		{
			DrawFolderTree(DirPath);
			ImGui::TreePop();
		}

		if (ImGui::IsItemHovered())
		{
			if (Entry.is_directory())
			{
				bIsMouseOnDirectory = true;
				DirectoryPathUnderMouse = DirPath;
				
			}
			else if (Entry.is_regular_file())
			{
				bIsMouseOnFile = true;
				FilePathUnderMouse = DirPath;
			}

		}
	}
}

void FContentBrowserWindow::DrawFileGrid()
{
	const float CellSize = 80.0f;
	const float IconSize = 48.0f;

	float PanelWidth = ImGui::GetContentRegionAvail().x;
	int ColumnCount = (int)(PanelWidth / CellSize);
	if (ColumnCount < 1) ColumnCount = 1;

	ImGui::Columns(ColumnCount, 0, false);

	for (auto& Entry : std::filesystem::directory_iterator(CurrentPath))
	{
		const auto& 
			Path = Entry.path();
		auto NameUtf8 = Path.filename().u8string();
		std::string Name(NameUtf8.begin(), NameUtf8.end());

		std::string Ext = Path.extension().string();
		std::ranges::transform(Ext, Ext.begin(), [](unsigned char c) {
			return std::tolower(c);
			});

		if (Entry.is_regular_file())
		{
			if (!(Ext == ".json" || Ext == ".obj"))
			{
				continue;
			}
		}

		/** PushID 전에 종료처리 할것 */

		ImGui::PushID(Name.c_str());

		ImTextureID Icon = Entry.is_directory() ? FolderIcon : FileIcon;

		// 아이콘 버튼
		ImGui::ImageButton(Name.c_str(), Icon, ImVec2(IconSize, IconSize));

		if (Entry.is_directory())
		{
		}
		else
		{
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Delete"))
				{
					std::filesystem::remove(Path);
				}
				ImGui::EndPopup();
			}			
		}

		// 선택
		if (ImGui::IsItemClicked())
		{
			SelectedPath = Path;
		}

		// 더블클릭 처리 🔥
		if (ImGui::IsItemHovered())
		{
			if (Entry.is_directory())
			{
				bIsMouseOnDirectory = true;
				DirectoryPathUnderMouse = Path;
			}
			else if (Entry.is_regular_file())
			{
				bIsMouseOnFile = true;
				FilePathUnderMouse = Path;
			}

			if (ImGui::IsMouseDoubleClicked(0))
			{
				if (Entry.is_directory())
				{
					CurrentPath /= Path.filename(); // 폴더 진입
				}
				else
				{
					OnFileDoubleClickCallback(Path.string());
				}
			}
			else if (!bFileOnDrag && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !Entry.is_directory())
			{
				bFileOnDrag = true;
				SelectedFilePath = Path;
			}
		}

		// 이름
		ImGui::TextWrapped("%s", Name.c_str());

		ImGui::NextColumn();
		ImGui::PopID();
	}

	ImGui::Columns(1);
}