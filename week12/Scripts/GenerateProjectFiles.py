"""
Generate Visual Studio solution/project files for JSEngine.

The generated project is intentionally self-contained: it wires generated
reflection code, NuGet package restore, static third-party library builds,
runtime DLL copy steps, game/editor configuration defines, and per-config file
exclusion rules.
"""

import hashlib
import os
import runpy
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent

SOLUTION_NAME = "JSEngine"
PROJECT_NAME = "JSEngine"
PROJECT_DIR_NAME = "JSEngine"
PROJECT_DIR = ROOT / PROJECT_DIR_NAME
PROJECT_GUID = "{55068e81-c0a0-49f9-ab7b-54aea968722b}"
ROOT_NAMESPACE = "Week2"

SOLUTION_GUID = "{4EBC5DD2-CECA-4722-9D19-87C7CB5F481B}"
VS_PROJECT_TYPE = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"

CONFIGURATIONS = [
	("Debug", "Win32"),
	("Release", "Win32"),
	("Debug", "x64"),
	("Release", "x64"),
	("ObjViewer", "x64"),
	("GameClientDebug", "x64"),
	("GameClientRelease", "x64"),
]

SOLUTION_CONFIGURATIONS = [
	("Debug", "x64", "Debug", "x64"),
	("Debug", "x86", "Debug", "Win32"),
	("GameClientDebug", "x64", "GameClientDebug", "x64"),
	("GameClientDebug", "x86", "GameClientDebug", "x64"),
	("GameClientRelease", "x64", "GameClientRelease", "x64"),
	("GameClientRelease", "x86", "GameClientRelease", "x64"),
	("ObjViewer", "x64", "ObjViewer", "x64"),
	("ObjViewer", "x86", "ObjViewer", "x64"),
	("Release", "x64", "Release", "x64"),
	("Release", "x86", "Release", "Win32"),
]

SOURCE_SCAN_DIRS = ["Source"]
THIRD_PARTY_SCAN_DIRS = [
	"ThirdParty\\ImGui",
	"ThirdParty\\SimpleJSON",
]
THIRD_PARTY_SINGLE_FILES = [
	"ThirdParty\\luajit\\src\\lua.h",
]
SHADER_SCAN_DIRS = ["Shaders"]
PCH_HEADER = "pch.h"
PCH_SOURCE = "pch.cpp"
ROOT_FILES = [PCH_HEADER, PCH_SOURCE, "main.cpp"]

SOURCE_EXTS = {".cpp", ".c", ".cc", ".cxx"}
HEADER_EXTS = {".h", ".hpp", ".hxx", ".inl"}
SHADER_EXTS = {".hlsl", ".hlsli"}
RESOURCE_EXTS = {".rc"}
NATVIS_EXTS = {".natvis"}
NONE_EXTS = {".natstepfilter", ".config"}

INCLUDE_PATHS_EDITOR = [
	"Source\\Engine",
	"Source",
	"ThirdParty",
	"ThirdParty\\SoLoud\\include",
	"ThirdParty\\RmlUi\\Include",
	"ThirdParty\\ImGui",
	"Source\\Editor",
	".",
]
INCLUDE_PATHS_GAME = [
	"Source\\Engine",
	"Source",
	"ThirdParty",
	"ThirdParty\\SoLoud\\include",
	"ThirdParty\\RmlUi\\Include",
	"ThirdParty\\ImGui",
	".",
]

NUGET_PACKAGES = [
	("directxtk_desktop_win10", "2025.10.28.2"),
]

GAME_CLIENT_CONFIGS = ("GameClientDebug", "GameClientRelease")
EDITOR_SELECTION_CPP = "Source\\Editor\\Selection\\SelectionManager.cpp"

NS = "http://schemas.microsoft.com/developer/msbuild/2003"


def xml_attr(**kwargs):
	return {key: value for key, value in kwargs.items() if value is not None}


def condition(config, platform):
	return f"'$(Configuration)|$(Platform)'=='{config}|{platform}'"


def add_text(parent, tag, text=None, **attrs):
	elem = ET.SubElement(parent, tag, xml_attr(**attrs))
	if text is not None:
		elem.text = text
	return elem


def run_reflection_generation():
	script = ROOT / "Scripts" / "GenerateReflection.py"
	if script.exists():
		print("Generating reflection code...")
		runpy.run_path(str(script), run_name="__main__")


def add_file(files, kind, rel_path):
	rel = rel_path.replace("/", "\\")
	if rel not in files[kind]:
		files[kind].append(rel)


def classify_file(files, full_path, project_dir):
	rel = full_path.relative_to(project_dir)
	rel_str = str(rel).replace("/", "\\")
	ext = full_path.suffix.lower()

	if ext in SOURCE_EXTS:
		add_file(files, "ClCompile", rel_str)
	elif ext in HEADER_EXTS:
		add_file(files, "ClInclude", rel_str)
	elif ext in RESOURCE_EXTS:
		add_file(files, "ResourceCompile", rel_str)
	elif ext in NATVIS_EXTS:
		add_file(files, "Natvis", rel_str)
	elif ext in NONE_EXTS:
		add_file(files, "None", rel_str)


def scan_tree(files, project_dir, rel_dir):
	root = project_dir / rel_dir
	if not root.exists():
		return

	for dirpath, _, filenames in os.walk(root):
		for fname in sorted(filenames):
			classify_file(files, Path(dirpath) / fname, project_dir)


def scan_shaders(files, project_dir):
	for shader_dir in SHADER_SCAN_DIRS:
		root = project_dir / shader_dir
		if not root.exists():
			continue
		for dirpath, _, filenames in os.walk(root):
			for fname in sorted(filenames):
				full = Path(dirpath) / fname
				if full.suffix.lower() in SHADER_EXTS:
					add_file(files, "None", str(full.relative_to(project_dir)).replace("/", "\\"))


def scan_files(project_dir):
	files = {
		"ClCompile": [],
		"ClInclude": [],
		"ResourceCompile": [],
		"Natvis": [],
		"None": [],
	}

	for scan_dir in SOURCE_SCAN_DIRS:
		scan_tree(files, project_dir, scan_dir)

	for scan_dir in THIRD_PARTY_SCAN_DIRS:
		scan_tree(files, project_dir, scan_dir)

	for single_file in THIRD_PARTY_SINGLE_FILES:
		full = project_dir / single_file
		if full.exists():
			classify_file(files, full, project_dir)

	scan_shaders(files, project_dir)

	for root_file in ROOT_FILES:
		full = project_dir / root_file
		if full.exists():
			classify_file(files, full, project_dir)

	for kind in files:
		files[kind] = sorted(files[kind])

	return files


def get_filter(rel_path):
	parts = rel_path.replace("/", "\\").rsplit("\\", 1)
	return parts[0] if len(parts) > 1 else ""


def collect_all_filters(files):
	filters = set()
	for file_list in files.values():
		for item in file_list:
			filt = get_filter(item)
			if not filt:
				continue
			parts = filt.split("\\")
			for index in range(1, len(parts) + 1):
				filters.add("\\".join(parts[:index]))
	return filters


def is_release_like(config):
	return config in {"Release", "ObjViewer", "GameClientDebug", "GameClientRelease"}


def is_game_client(config):
	return config in GAME_CLIENT_CONFIGS


def get_include_paths(config):
	return INCLUDE_PATHS_GAME if is_game_client(config) else INCLUDE_PATHS_EDITOR


def get_definitions(config, platform):
	defs = []
	if platform == "Win32":
		defs.append("WIN32")

	defs.append("NDEBUG" if is_release_like(config) else "_DEBUG")
	defs.append("_CONSOLE")

	if is_game_client(config):
		defs += ["WITH_EDITOR=0", "IS_GAME_CLIENT=1", "IS_OBJ_VIEWER=0"]
		if config == "GameClientDebug":
			defs.append("GAME_DEVELOPMENT=1")
	else:
		defs.append("WITH_EDITOR=1")

	defs += [
		"FBXSDK_SHARED",
		"WITH_MINIAUDIO",
		"RMLUI_STATIC_LIB",
		"_CRT_SECURE_NO_WARNINGS",
		"NOMINMAX",
	]

	if platform == "x64":
		defs += ["SOL_LUA_VERSION=501", "SOL_LUAJIT=1"]

	if config == "ObjViewer":
		defs.append("IS_OBJ_VIEWER=1")

	defs.append("%(PreprocessorDefinitions)")
	return ";".join(defs) + ";"


def needs_luajit(config, platform):
	return platform == "x64" and config != "ObjViewer"


def needs_fbx(config, platform):
	return platform == "x64" and config != "ObjViewer"


def cl_additional_includes(config, platform):
	includes = []
	if is_game_client(config):
		includes.append("$(ProjectDir)Source\\Editor")
	if needs_luajit(config, platform):
		includes.append("$(ProjectDir)ThirdParty\\luajit\\src")
	includes += [
		"$(ProjectDir)ThirdParty\\SoLoud\\include",
		"$(ProjectDir)ThirdParty\\RmlUi\\Include",
		"$(ProjectDir)ThirdParty",
	]
	if needs_fbx(config, platform):
		includes.append("$(ProjectDir)ThirdParty\\FBX\\include")
	includes.append("%(AdditionalIncludeDirectories)")
	return ";".join(includes)


def link_library_dirs(config, platform):
	dirs = [
		"$(ProjectDir)packages\\directxtk_desktop_win10.2025.10.28.2\\native\\lib\\$(DirectXTKPlatform)\\$(DirectXTKBinaryConfiguration)",
		"$(ProjectDir)ThirdParty\\RmlUi\\Lib\\$(Platform)\\$(ThirdPartyBinaryConfiguration)",
		"$(ProjectDir)ThirdParty\\SoLoud\\Lib\\$(Platform)\\$(ThirdPartyBinaryConfiguration)",
	]
	if needs_fbx(config, platform):
		dirs.append("$(ProjectDir)ThirdParty\\FBX\\lib\\$(FbxSdkConfiguration)")
	if needs_luajit(config, platform):
		dirs.append("$(ProjectDir)ThirdParty\\luajit\\src")
	dirs.append("%(AdditionalLibraryDirectories)")
	return ";".join(dirs)


def link_dependencies(config, platform):
	deps = ["DirectXTK.lib", "RmlUiCore.lib", "SoLoud.lib"]
	if needs_fbx(config, platform):
		deps.append("libfbxsdk.lib")
	if needs_luajit(config, platform):
		deps += ["gdiplus.lib", "lua51.lib"]
	deps.append("%(AdditionalDependencies)")
	return ";".join(deps)


def should_exclude_from_game_client(rel_path):
	if rel_path.startswith("Source\\Misc\\ObjViewer\\"):
		return True
	if rel_path.startswith("Source\\Editor\\") and rel_path != EDITOR_SELECTION_CPP:
		return True
	return False


def add_game_client_exclusions(elem, rel_path):
	if not should_exclude_from_game_client(rel_path):
		return
	for config in GAME_CLIENT_CONFIGS:
		add_text(elem, "ExcludedFromBuild", "true", Condition=condition(config, "x64"))


def add_resource_exclusions(elem, rel_path):
	if rel_path.endswith("Source\\Engine\\Runtime\\AppIcon.rc"):
		for config, platform in [
			("Debug", "Win32"),
			("Release", "Win32"),
			("ObjViewer", "x64"),
			("GameClientDebug", "x64"),
			("GameClientRelease", "x64"),
		]:
			add_text(elem, "ExcludedFromBuild", "true", Condition=condition(config, platform))
	elif rel_path.endswith("Source\\Engine\\Runtime\\GameBranding.rc"):
		for config, platform in [
			("Debug", "x64"),
			("Release", "x64"),
		]:
			add_text(elem, "ExcludedFromBuild", "true", Condition=condition(config, platform))


def indent_xml(elem, level=0):
	indent = "\n" + "  " * level
	if len(elem):
		if not elem.text or not elem.text.strip():
			elem.text = indent + "  "
		for child in elem:
			indent_xml(child, level + 1)
		if not child.tail or not child.tail.strip():
			child.tail = indent
	if level and (not elem.tail or not elem.tail.strip()):
		elem.tail = indent
	if level == 0:
		elem.tail = "\n"


def write_xml(root_elem, filepath, bom=False):
	indent_xml(root_elem)
	tree = ET.ElementTree(root_elem)
	encoding = "utf-8-sig" if bom else "utf-8"
	with open(filepath, "w", encoding=encoding, newline="\r\n") as f:
		f.write('<?xml version="1.0" encoding="utf-8"?>\n')
		tree.write(f, encoding="unicode", xml_declaration=False)


def add_project_configuration_group(proj):
	item_group = add_text(proj, "ItemGroup", Label="ProjectConfigurations")
	for config, platform in CONFIGURATIONS:
		pc = add_text(item_group, "ProjectConfiguration", Include=f"{config}|{platform}")
		add_text(pc, "Configuration", config)
		add_text(pc, "Platform", platform)


def add_configuration_properties(proj):
	for config, platform in CONFIGURATIONS:
		pg = add_text(proj, "PropertyGroup", Condition=condition(config, platform), Label="Configuration")
		add_text(pg, "ConfigurationType", "Application")
		add_text(pg, "UseDebugLibraries", "false" if is_release_like(config) else "true")
		add_text(pg, "PlatformToolset", "v143")
		if is_release_like(config):
			add_text(pg, "WholeProgramOptimization", "true")
		add_text(pg, "CharacterSet", "Unicode")


def add_property_sheets(proj):
	for config, platform in CONFIGURATIONS:
		group = add_text(proj, "ImportGroup", Label="PropertySheets", Condition=condition(config, platform))
		add_text(
			group,
			"Import",
			Project="$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props",
			Condition="exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')",
			Label="LocalAppDataPlatform",
		)


def add_config_property_groups(proj):
	for config, platform in CONFIGURATIONS:
		pg = add_text(proj, "PropertyGroup", Condition=condition(config, platform))
		add_text(pg, "OutDir", "$(ProjectDir)Bin\\$(Configuration)\\")
		add_text(pg, "IntDir", "$(ProjectDir)Build\\$(Configuration)\\")
		if is_game_client(config):
			add_text(pg, "TargetName", "JSEngineGame")
		add_text(pg, "IncludePath", ";".join(get_include_paths(config)) + ";$(IncludePath)")
		add_text(pg, "LibraryPath", "$(LibraryPath)")
		add_text(pg, "LocalDebuggerWorkingDirectory", "$(ProjectDir)")


def add_item_definition_groups(proj):
	for config, platform in CONFIGURATIONS:
		idg = add_text(proj, "ItemDefinitionGroup", Condition=condition(config, platform))
		cl = add_text(idg, "ClCompile")
		add_text(cl, "WarningLevel", "Level3")
		if is_release_like(config):
			add_text(cl, "FunctionLevelLinking", "true")
			add_text(cl, "IntrinsicFunctions", "true")
		add_text(cl, "SDLCheck", "true")
		add_text(cl, "PreprocessorDefinitions", get_definitions(config, platform))
		add_text(cl, "MultiProcessorCompilation", "true")
		add_text(cl, "ConformanceMode", "true")
		add_text(cl, "PrecompiledHeader", "Use")
		add_text(cl, "PrecompiledHeaderFile", PCH_HEADER)
		add_text(cl, "ForcedIncludeFiles", f"{PCH_HEADER};%(ForcedIncludeFiles)")
		add_text(cl, "AdditionalOptions", "/utf-8 /bigobj %(AdditionalOptions)")
		add_text(cl, "ExceptionHandling", "Async")
		if platform == "x64":
			add_text(cl, "LanguageStandard", "stdcpp20")
		add_text(
			cl,
			"AdditionalIncludeDirectories",
			"$(ProjectDir)packages\\directxtk_desktop_win10.2025.10.28.2\\include;" + cl_additional_includes(config, platform),
		)

		link = add_text(idg, "Link")
		add_text(link, "SubSystem", "Windows" if platform == "x64" else "Console")
		add_text(link, "GenerateDebugInformation", "true")
		add_text(link, "AdditionalLibraryDirectories", link_library_dirs(config, platform))
		add_text(link, "AdditionalDependencies", link_dependencies(config, platform))


def add_shared_build_properties(proj):
	pg = add_text(proj, "PropertyGroup")
	add_text(pg, "ThirdPartyBinaryConfiguration", "Release")
	add_text(pg, "ThirdPartyBinaryConfiguration", "Debug", Condition="'$(Configuration)'=='Debug'")
	add_text(pg, "FbxSdkConfiguration", "release")
	add_text(pg, "FbxSdkConfiguration", "debug", Condition="'$(Configuration)'=='Debug'")
	add_text(pg, "DirectXTKBinaryConfiguration", "Release")
	add_text(pg, "DirectXTKBinaryConfiguration", "Debug", Condition="'$(Configuration)'=='Debug'")
	add_text(pg, "DirectXTKPlatform", "$(Platform)")
	add_text(pg, "DirectXTKPlatform", "x86", Condition="'$(Platform)'=='Win32'")


def add_build_targets(proj):
	restore = add_text(
		proj,
		"Target",
		Name="RestoreNuGetPackages",
		BeforeTargets="PrepareForBuild",
		Condition="!Exists('$(ProjectDir)packages\\directxtk_desktop_win10.2025.10.28.2\\build\\native\\directxtk_desktop_win10.targets')",
	)
	restore_command = (
		"powershell -NoProfile -ExecutionPolicy Bypass -Command "
		"\"$ErrorActionPreference='Stop'; "
		"$toolsDir = Join-Path '$(MSBuildProjectDirectory)' 'Build\\NuGet'; "
		"$nuget = Join-Path $toolsDir 'nuget.exe'; "
		"if (!(Test-Path $nuget)) { "
		"New-Item -ItemType Directory -Force -Path $toolsDir | Out-Null; "
		"Invoke-WebRequest -Uri 'https://dist.nuget.org/win-x86-commandline/latest/nuget.exe' -OutFile $nuget "
		"}; "
		"& $nuget restore '$(MSBuildProjectDirectory)\\packages.config' "
		"-PackagesDirectory '$(MSBuildProjectDirectory)\\packages' "
		"-ConfigFile '$(MSBuildProjectDirectory)\\..\\nuget.config'\""
	)
	add_text(restore, "Exec", Command=restore_command)

	rml = add_text(proj, "Target", Name="BuildRmlUiStaticLibrary", BeforeTargets="ClCompile")
	add_text(
		rml,
		"Exec",
		Command='powershell -NoProfile -ExecutionPolicy Bypass -File "$(MSBuildProjectDirectory)\\BuildTools\\Scripts\\BuildRmlUiLib.ps1" -ProjectDir "$(MSBuildProjectDirectory)" -Configuration "$(ThirdPartyBinaryConfiguration)" -Platform "$(Platform)"',
	)

	soloud = add_text(proj, "Target", Name="BuildSoLoudStaticLibrary", BeforeTargets="ClCompile")
	add_text(
		soloud,
		"Exec",
		Command='powershell -NoProfile -ExecutionPolicy Bypass -File "$(MSBuildProjectDirectory)\\BuildTools\\Scripts\\BuildSoLoudLib.ps1" -ProjectDir "$(MSBuildProjectDirectory)" -Configuration "$(ThirdPartyBinaryConfiguration)" -Platform "$(Platform)"',
	)

	reflection = add_text(proj, "Target", Name="GenerateReflectionCode", BeforeTargets="ClCompile")
	add_text(
		reflection,
		"Exec",
		Command='"$(MSBuildProjectDirectory)\\..\\Scripts\\python\\python.exe" "$(MSBuildProjectDirectory)\\..\\Scripts\\GenerateReflection.py"',
	)


def add_item_groups(proj, files):
	group = add_text(proj, "ItemGroup")
	for rel_path in files["ClCompile"]:
		item = add_text(group, "ClCompile", Include=rel_path)
		if rel_path == PCH_SOURCE:
			add_text(item, "PrecompiledHeader", "Create")
		add_game_client_exclusions(item, rel_path)

	group = add_text(proj, "ItemGroup")
	for rel_path in files["ClInclude"]:
		add_text(group, "ClInclude", Include=rel_path)

	if files["None"]:
		group = add_text(proj, "ItemGroup")
		for rel_path in files["None"]:
			add_text(group, "None", Include=rel_path)

	if files["ResourceCompile"]:
		group = add_text(proj, "ItemGroup")
		for rel_path in files["ResourceCompile"]:
			item = add_text(group, "ResourceCompile", Include=rel_path)
			add_resource_exclusions(item, rel_path)

	if files["Natvis"]:
		group = add_text(proj, "ItemGroup")
		for rel_path in files["Natvis"]:
			add_text(group, "Natvis", Include=rel_path)


def add_post_targets_and_nuget_imports(proj):
	copy_lua = add_text(
		proj,
		"Target",
		Name="CopyLuaJitRuntimeDll",
		AfterTargets="Build",
		Condition="Exists('$(ProjectDir)ThirdParty\\luajit\\src\\lua51.dll')",
	)
	add_text(
		copy_lua,
		"Copy",
		SourceFiles="$(ProjectDir)ThirdParty\\luajit\\src\\lua51.dll",
		DestinationFolder="$(OutDir)",
		SkipUnchangedFiles="true",
	)

	copy_fbx = add_text(
		proj,
		"Target",
		Name="CopyFbxSdkRuntimeDll",
		AfterTargets="Build",
		Condition="'$(Platform)'=='x64' and '$(Configuration)'!='ObjViewer' and Exists('$(ProjectDir)ThirdParty\\FBX\\lib\\$(FbxSdkConfiguration)\\libfbxsdk.dll')",
	)
	add_text(
		copy_fbx,
		"Copy",
		SourceFiles="$(ProjectDir)ThirdParty\\FBX\\lib\\$(FbxSdkConfiguration)\\libfbxsdk.dll",
		DestinationFolder="$(OutDir)",
		SkipUnchangedFiles="true",
	)

	import_group = add_text(proj, "ImportGroup", Label="ExtensionTargets")
	for package_id, version in NUGET_PACKAGES:
		target_path = f"packages\\{package_id}.{version}\\build\\native\\{package_id}.targets"
		add_text(import_group, "Import", Project=target_path, Condition=f"Exists('{target_path}')")

	ensure = add_text(
		proj,
		"Target",
		Name="EnsureNuGetPackageBuildImports",
		BeforeTargets="PrepareForBuild",
		DependsOnTargets="RestoreNuGetPackages",
	)
	pg = add_text(ensure, "PropertyGroup")
	add_text(
		pg,
		"ErrorText",
		"This project references NuGet package(s) that are missing and automatic restore failed. "
		"Restore NuGet packages or build again with network access. Missing file: {0}.",
	)
	for package_id, version in NUGET_PACKAGES:
		target_path = f"packages\\{package_id}.{version}\\build\\native\\{package_id}.targets"
		add_text(
			ensure,
			"Error",
			Condition=f"!Exists('{target_path}')",
			Text=f"$([System.String]::Format('$(ErrorText)', '{target_path}'))",
		)


def generate_vcxproj(files):
	proj = ET.Element("Project", DefaultTargets="Build", xmlns=NS)

	add_project_configuration_group(proj)

	globals_group = add_text(proj, "PropertyGroup", Label="Globals")
	add_text(globals_group, "VCProjectVersion", "17.0")
	add_text(globals_group, "Keyword", "Win32Proj")
	add_text(globals_group, "ProjectGuid", PROJECT_GUID)
	add_text(globals_group, "RootNamespace", ROOT_NAMESPACE)
	add_text(globals_group, "WindowsTargetPlatformVersion", "10.0")

	add_text(proj, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props")
	add_configuration_properties(proj)
	add_text(proj, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.props")
	add_text(proj, "ImportGroup", Label="ExtensionSettings")
	add_text(proj, "ImportGroup", Label="Shared")
	add_property_sheets(proj)
	add_text(proj, "PropertyGroup", Label="UserMacros")
	add_config_property_groups(proj)
	add_shared_build_properties(proj)
	add_item_definition_groups(proj)
	add_build_targets(proj)
	add_item_groups(proj, files)
	add_text(proj, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.targets")
	add_post_targets_and_nuget_imports(proj)

	write_xml(proj, PROJECT_DIR / f"{PROJECT_NAME}.vcxproj")


def generate_filters(files):
	proj = ET.Element("Project", ToolsVersion="4.0", xmlns=NS)

	all_filters = collect_all_filters(files)
	if all_filters:
		group = add_text(proj, "ItemGroup")
		for filt in sorted(all_filters):
			elem = add_text(group, "Filter", Include=filt)
			digest = hashlib.md5(f"{PROJECT_NAME}:{filt}".encode()).hexdigest()
			uid = f"{{{digest[:8]}-{digest[8:12]}-{digest[12:16]}-{digest[16:20]}-{digest[20:32]}}}"
			add_text(elem, "UniqueIdentifier", uid)

	for kind in ["ClCompile", "ClInclude", "None", "ResourceCompile", "Natvis"]:
		if not files[kind]:
			continue
		group = add_text(proj, "ItemGroup")
		for rel_path in files[kind]:
			elem = add_text(group, kind, Include=rel_path)
			filt = get_filter(rel_path)
			if filt:
				add_text(elem, "Filter", filt)

	write_xml(proj, PROJECT_DIR / f"{PROJECT_NAME}.vcxproj.filters", bom=True)


def generate_sln():
	lines = [
		"",
		"Microsoft Visual Studio Solution File, Format Version 12.00",
		"# Visual Studio Version 17",
		"VisualStudioVersion = 17.14.37012.4",
		"MinimumVisualStudioVersion = 10.0.40219.1",
	]

	guid_upper = PROJECT_GUID.upper()
	lines.append(
		f'Project("{VS_PROJECT_TYPE}") = "{PROJECT_NAME}", '
		f'"{PROJECT_DIR_NAME}\\{PROJECT_NAME}.vcxproj", "{guid_upper}"'
	)
	lines.append("EndProject")
	lines.append("Global")

	lines.append("\tGlobalSection(SolutionConfigurationPlatforms) = preSolution")
	for config, sln_platform, _, _ in SOLUTION_CONFIGURATIONS:
		lines.append(f"\t\t{config}|{sln_platform} = {config}|{sln_platform}")
	lines.append("\tEndGlobalSection")

	lines.append("\tGlobalSection(ProjectConfigurationPlatforms) = postSolution")
	for config, sln_platform, project_config, project_platform in SOLUTION_CONFIGURATIONS:
		lines.append(f"\t\t{guid_upper}.{config}|{sln_platform}.ActiveCfg = {project_config}|{project_platform}")
		lines.append(f"\t\t{guid_upper}.{config}|{sln_platform}.Build.0 = {project_config}|{project_platform}")
	lines.append("\tEndGlobalSection")

	lines += [
		"\tGlobalSection(SolutionProperties) = preSolution",
		"\t\tHideSolutionNode = FALSE",
		"\tEndGlobalSection",
		"\tGlobalSection(ExtensibilityGlobals) = postSolution",
		f"\t\tSolutionGuid = {SOLUTION_GUID}",
		"\tEndGlobalSection",
		"EndGlobal",
		"",
	]

	with open(ROOT / f"{SOLUTION_NAME}.sln", "w", encoding="utf-8", newline="\r\n") as f:
		f.write("\n".join(lines))


def main():
	print(f"Scanning project files in {PROJECT_DIR}...")

	run_reflection_generation()
	files = scan_files(PROJECT_DIR)

	print(f"  ClCompile:        {len(files['ClCompile'])} files")
	print(f"  ClInclude:        {len(files['ClInclude'])} files")
	print(f"  ResourceCompile:  {len(files['ResourceCompile'])} files")
	print(f"  Natvis:           {len(files['Natvis'])} files")
	print(f"  None:             {len(files['None'])} files")

	print("Generating project files...")
	generate_vcxproj(files)
	print(f"  {PROJECT_NAME}.vcxproj")
	generate_filters(files)
	print(f"  {PROJECT_NAME}.vcxproj.filters")
	generate_sln()
	print(f"  {SOLUTION_NAME}.sln")
	print("Done!")


if __name__ == "__main__":
	main()
