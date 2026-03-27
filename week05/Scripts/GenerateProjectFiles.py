"""
GenerateProjectFiles.py — Auto-generate .sln, .vcxproj, .vcxproj.filters
for KraftonJungleEngine from the on-disk folder structure.

Usage:
    python Scripts/GenerateProjectFiles.py
"""

import os
import xml.etree.ElementTree as ET
from pathlib import Path

# ──────────────────────────────────────────────
# Constants
# ──────────────────────────────────────────────
ROOT = Path(__file__).resolve().parent.parent

SOLUTION_GUID = "{69068260-58EF-4950-9F0D-EC842BBF9C39}"
VS_PROJECT_TYPE = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"

PROJECTS = {
    "Engine": {
        "guid": "{135D1BDB-24F8-4FBC-959A-F422D47E0261}",
        "root_namespace": "Engine",
        "dependencies": [],
    },
    "Editor": {
        "guid": "{784282C7-5128-4F6A-923B-1DE0BE007C0A}",
        "root_namespace": "Editor",
        "dependencies": ["{135D1BDB-24F8-4FBC-959A-F422D47E0261}"],
    },
    "Client": {
        "guid": "{31536DD1-487B-419F-AA5A-1A6B996A7BEF}",
        "root_namespace": "Client",
        "dependencies": ["{135D1BDB-24F8-4FBC-959A-F422D47E0261}"],
    },
}

CONFIGURATIONS = [
    ("Debug", "Win32"),
    ("Release", "Win32"),
    ("Debug", "x64"),
    ("Release", "x64"),
]

# File extensions to include
SOURCE_EXTS = {".cpp", ".c", ".cc", ".cxx"}
HEADER_EXTS = {".h", ".hpp", ".hxx", ".inl"}
NATVIS_EXTS = {".natvis"}
NONE_EXTS = {".natstepfilter", ".config"}

NS = "http://schemas.microsoft.com/developer/msbuild/2003"


# ──────────────────────────────────────────────
# Helpers
# ──────────────────────────────────────────────
def scan_files(project_dir: Path, scan_dirs: list[str]) -> dict[str, list[str]]:
    """Scan directories and collect files grouped by type."""
    result = {"ClCompile": [], "ClInclude": [], "Natvis": [], "None": []}

    for scan_dir in scan_dirs:
        full_dir = project_dir / scan_dir
        if not full_dir.exists():
            continue
        for dirpath, _, filenames in os.walk(full_dir):
            for fname in sorted(filenames):
                full = Path(dirpath) / fname
                rel = full.relative_to(project_dir)
                # Use backslash for Windows paths in vcxproj
                rel_str = str(rel).replace("/", "\\")
                ext = full.suffix.lower()

                if ext in SOURCE_EXTS:
                    result["ClCompile"].append(rel_str)
                elif ext in HEADER_EXTS:
                    result["ClInclude"].append(rel_str)
                elif ext in NATVIS_EXTS:
                    result["Natvis"].append(rel_str)
                elif ext in NONE_EXTS:
                    result["None"].append(rel_str)

    return result


def get_filter(rel_path: str) -> str:
    """Return the filter (directory portion) from a relative path."""
    parts = rel_path.replace("/", "\\").rsplit("\\", 1)
    return parts[0] if len(parts) > 1 else ""


def collect_all_filters(files: dict[str, list[str]]) -> set[str]:
    """Collect all unique filter paths including parent paths."""
    filters = set()
    for file_list in files.values():
        for f in file_list:
            filt = get_filter(f)
            if filt:
                # Add all parent paths too
                parts = filt.split("\\")
                for i in range(1, len(parts) + 1):
                    filters.add("\\".join(parts[:i]))
    return filters


# ──────────────────────────────────────────────
# XML Generation
# ──────────────────────────────────────────────
def indent_xml(elem, level=0):
    """Add indentation to XML tree."""
    i = "\n" + "  " * level
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = i + "  "
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
        for child in elem:
            indent_xml(child, level + 1)
        if not child.tail or not child.tail.strip():
            child.tail = i
    else:
        if level and (not elem.tail or not elem.tail.strip()):
            elem.tail = i
    if level == 0:
        elem.tail = "\n"


def write_xml(root_elem, filepath: Path, bom=False):
    """Write XML tree to file with proper declaration."""
    indent_xml(root_elem)
    tree = ET.ElementTree(root_elem)
    with open(filepath, "w", encoding="utf-8", newline="\r\n") as f:
        if bom:
            f.write("\ufeff")
        f.write('<?xml version="1.0" encoding="utf-8"?>\n')
        tree.write(f, encoding="unicode", xml_declaration=False)


# ──────────────────────────────────────────────
# Engine .vcxproj
# ──────────────────────────────────────────────
def generate_engine_vcxproj(files: dict[str, list[str]]):
    proj = ET.Element("Project", DefaultTargets="Build", xmlns=NS)

    # ProjectConfigurations
    ig = ET.SubElement(proj, "ItemGroup", Label="ProjectConfigurations")
    for cfg, plat in CONFIGURATIONS:
        pc = ET.SubElement(ig, "ProjectConfiguration", Include=f"{cfg}|{plat}")
        ET.SubElement(pc, "Configuration").text = cfg
        ET.SubElement(pc, "Platform").text = plat

    # ClInclude
    ig = ET.SubElement(proj, "ItemGroup")
    for f in files["ClInclude"]:
        ET.SubElement(ig, "ClInclude", Include=f)

    # ClCompile
    ig = ET.SubElement(proj, "ItemGroup")
    for f in files["ClCompile"]:
        ET.SubElement(ig, "ClCompile", Include=f)

    # Globals
    pg = ET.SubElement(proj, "PropertyGroup", Label="Globals")
    ET.SubElement(pg, "VCProjectVersion").text = "17.0"
    ET.SubElement(pg, "Keyword").text = "Win32Proj"
    ET.SubElement(pg, "ProjectGuid").text = "{135d1bdb-24f8-4fbc-959a-f422d47e0261}"
    ET.SubElement(pg, "RootNamespace").text = "Engine"
    ET.SubElement(pg, "WindowsTargetPlatformVersion").text = "10.0"

    ET.SubElement(proj, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props")

    # Configuration properties
    for cfg, plat in CONFIGURATIONS:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        pg = ET.SubElement(proj, "PropertyGroup", Condition=cond, Label="Configuration")
        is_x64 = plat == "x64"
        is_release = cfg == "Release"
        ET.SubElement(pg, "ConfigurationType").text = "DynamicLibrary" if is_x64 else "Application"
        ET.SubElement(pg, "UseDebugLibraries").text = "false" if is_release else "true"
        ET.SubElement(pg, "PlatformToolset").text = "v143"
        if is_release:
            ET.SubElement(pg, "WholeProgramOptimization").text = "true"
        ET.SubElement(pg, "CharacterSet").text = "Unicode"

    ET.SubElement(proj, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.props")
    ET.SubElement(proj, "ImportGroup", Label="ExtensionSettings")
    ET.SubElement(proj, "ImportGroup", Label="Shared")

    # PropertySheets
    for cfg, plat in CONFIGURATIONS:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        ig = ET.SubElement(proj, "ImportGroup", Label="PropertySheets", Condition=cond)
        ET.SubElement(ig, "Import",
                      Project="$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props",
                      Condition="exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')",
                      Label="LocalAppDataPlatform")

    ET.SubElement(proj, "PropertyGroup", Label="UserMacros")

    # Output/Intermediate dirs for x64
    for cfg in ("Debug", "Release"):
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|x64'"
        pg = ET.SubElement(proj, "PropertyGroup", Condition=cond)
        ET.SubElement(pg, "OutDir").text = "$(ProjectDir)Bin\\$(Configuration)\\"
        ET.SubElement(pg, "IntDir").text = "Build\\$(Configuration)\\"

    # ItemDefinitionGroups
    for cfg, plat in CONFIGURATIONS:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        idg = ET.SubElement(proj, "ItemDefinitionGroup", Condition=cond)
        cl = ET.SubElement(idg, "ClCompile")
        ET.SubElement(cl, "WarningLevel").text = "Level3"

        is_x64 = plat == "x64"
        is_release = cfg == "Release"
        is_win32 = plat == "Win32"

        if is_release:
            ET.SubElement(cl, "FunctionLevelLinking").text = "true"
            ET.SubElement(cl, "IntrinsicFunctions").text = "true"

        ET.SubElement(cl, "SDLCheck").text = "true"

        if is_win32:
            defs = f"WIN32;{'NDEBUG' if is_release else '_DEBUG'};_CONSOLE;%(PreprocessorDefinitions)"
        else:
            defs = f"NOMINMAX;ENGINECORE_EXPORTS;{'NDEBUG' if is_release else '_DEBUG'};_CONSOLE;%(PreprocessorDefinitions)"
        ET.SubElement(cl, "PreprocessorDefinitions").text = defs

        ET.SubElement(cl, "ConformanceMode").text = "true"

        if is_x64:
            ET.SubElement(cl, "LanguageStandard").text = "stdcpp20"
            ET.SubElement(cl, "AdditionalOptions").text = "/utf-8 %(AdditionalOptions)"
            ET.SubElement(cl, "AdditionalIncludeDirectories").text = "$(ProjectDir)Source\\;$(ProjectDir);%(AdditionalIncludeDirectories)"

        link = ET.SubElement(idg, "Link")
        ET.SubElement(link, "SubSystem").text = "Console"
        ET.SubElement(link, "GenerateDebugInformation").text = "true"

    # packages.config None item
    ig = ET.SubElement(proj, "ItemGroup")
    ET.SubElement(ig, "None", Include="packages.config")

    ET.SubElement(proj, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.targets")

    # NuGet DirectXTK
    ig = ET.SubElement(proj, "ImportGroup", Label="ExtensionTargets")
    ET.SubElement(ig, "Import",
                  Project="..\\packages\\directxtk_desktop_win10.2025.10.28.2\\build\\native\\directxtk_desktop_win10.targets",
                  Condition="Exists('..\\packages\\directxtk_desktop_win10.2025.10.28.2\\build\\native\\directxtk_desktop_win10.targets')")

    target = ET.SubElement(proj, "Target", Name="EnsureNuGetPackageBuildImports", BeforeTargets="PrepareForBuild")
    tpg = ET.SubElement(target, "PropertyGroup")
    ET.SubElement(tpg, "ErrorText").text = (
        "이 프로젝트는 이 컴퓨터에 없는 NuGet 패키지를 참조합니다. "
        "해당 패키지를 다운로드하려면 NuGet 패키지 복원을 사용하십시오. "
        "자세한 내용은 http://go.microsoft.com/fwlink/?LinkID=322105를 참조하십시오. "
        "누락된 파일은 {0}입니다."
    )
    ET.SubElement(target, "Error",
                  Condition="!Exists('..\\packages\\directxtk_desktop_win10.2025.10.28.2\\build\\native\\directxtk_desktop_win10.targets')",
                  Text="$([System.String]::Format('$(ErrorText)', '..\\packages\\directxtk_desktop_win10.2025.10.28.2\\build\\native\\directxtk_desktop_win10.targets'))")

    write_xml(proj, ROOT / "Engine" / "Engine.vcxproj")


# ──────────────────────────────────────────────
# Editor .vcxproj
# ──────────────────────────────────────────────
def generate_editor_vcxproj(files: dict[str, list[str]]):
    proj = ET.Element("Project", DefaultTargets="Build", xmlns=NS)

    # ProjectConfigurations
    ig = ET.SubElement(proj, "ItemGroup", Label="ProjectConfigurations")
    for cfg, plat in CONFIGURATIONS:
        pc = ET.SubElement(ig, "ProjectConfiguration", Include=f"{cfg}|{plat}")
        ET.SubElement(pc, "Configuration").text = cfg
        ET.SubElement(pc, "Platform").text = plat

    # Globals
    pg = ET.SubElement(proj, "PropertyGroup", Label="Globals")
    ET.SubElement(pg, "VCProjectVersion").text = "17.0"
    ET.SubElement(pg, "Keyword").text = "Win32Proj"
    ET.SubElement(pg, "ProjectGuid").text = "{784282c7-5128-4f6a-923b-1de0be007c0a}"
    ET.SubElement(pg, "RootNamespace").text = "Editor"
    ET.SubElement(pg, "WindowsTargetPlatformVersion").text = "10.0"

    ET.SubElement(proj, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props")

    # Configuration properties
    for cfg, plat in CONFIGURATIONS:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        pg = ET.SubElement(proj, "PropertyGroup", Condition=cond, Label="Configuration")
        is_release = cfg == "Release"
        ET.SubElement(pg, "ConfigurationType").text = "Application"
        ET.SubElement(pg, "UseDebugLibraries").text = "false" if is_release else "true"
        ET.SubElement(pg, "PlatformToolset").text = "v143"
        if is_release:
            ET.SubElement(pg, "WholeProgramOptimization").text = "true"
        ET.SubElement(pg, "CharacterSet").text = "Unicode"

    ET.SubElement(proj, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.props")
    ET.SubElement(proj, "ImportGroup", Label="ExtensionSettings")
    ET.SubElement(proj, "ImportGroup", Label="Shared")

    # PropertySheets
    for cfg, plat in CONFIGURATIONS:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        ig = ET.SubElement(proj, "ImportGroup", Label="PropertySheets", Condition=cond)
        ET.SubElement(ig, "Import",
                      Project="$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props",
                      Condition="exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')",
                      Label="LocalAppDataPlatform")

    ET.SubElement(proj, "PropertyGroup", Label="UserMacros")

    # Output/Intermediate dirs + DisableFastUpToDateCheck for x64
    for cfg in ("Debug", "Release"):
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|x64'"
        pg = ET.SubElement(proj, "PropertyGroup", Condition=cond)
        ET.SubElement(pg, "OutDir").text = "$(ProjectDir)Bin\\$(Configuration)\\"
        ET.SubElement(pg, "IntDir").text = "Build\\$(Configuration)\\"
        ET.SubElement(pg, "DisableFastUpToDateCheck").text = "true"

    # ItemDefinitionGroups
    for cfg, plat in CONFIGURATIONS:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        idg = ET.SubElement(proj, "ItemDefinitionGroup", Condition=cond)
        cl = ET.SubElement(idg, "ClCompile")
        ET.SubElement(cl, "WarningLevel").text = "Level3"

        is_x64 = plat == "x64"
        is_release = cfg == "Release"
        is_win32 = plat == "Win32"

        if is_release:
            ET.SubElement(cl, "FunctionLevelLinking").text = "true"
            ET.SubElement(cl, "IntrinsicFunctions").text = "true"

        ET.SubElement(cl, "SDLCheck").text = "true"

        if is_win32:
            defs = f"WIN32;{'NDEBUG' if is_release else '_DEBUG'};_CONSOLE;%(PreprocessorDefinitions)"
        else:
            defs = f"{'NDEBUG' if is_release else '_DEBUG'};_CONSOLE;%(PreprocessorDefinitions)"
        ET.SubElement(cl, "PreprocessorDefinitions").text = defs

        ET.SubElement(cl, "ConformanceMode").text = "true"

        if is_x64:
            ET.SubElement(cl, "LanguageStandard").text = "stdcpp20"
            ET.SubElement(cl, "AdditionalOptions").text = "/utf-8 %(AdditionalOptions)"
            ET.SubElement(cl, "AdditionalIncludeDirectories").text = (
                "$(ProjectDir)Source\\;$(ProjectDir)..\\Engine\\Source\\;"
                "$(ProjectDir)ThirdParty\\imgui;%(AdditionalIncludeDirectories)"
            )

        link = ET.SubElement(idg, "Link")
        if is_x64:
            ET.SubElement(link, "SubSystem").text = "Windows"
            ET.SubElement(link, "GenerateDebugInformation").text = "true"
            ET.SubElement(link, "AdditionalLibraryDirectories").text = (
                "$(ProjectDir)..\\Engine\\Bin\\$(Configuration)\\;%(AdditionalLibraryDirectories)"
            )
            ET.SubElement(link, "AdditionalDependencies").text = "Engine.lib;%(AdditionalDependencies)"

            pbe = ET.SubElement(idg, "PostBuildEvent")
            ET.SubElement(pbe, "Command").text = (
                'copy /Y "$(ProjectDir)..\\Engine\\Bin\\$(Configuration)\\Engine.dll" "$(OutDir)Engine.dll"'
            )
        else:
            ET.SubElement(link, "SubSystem").text = "Console"
            ET.SubElement(link, "GenerateDebugInformation").text = "true"

    # File items
    ig = ET.SubElement(proj, "ItemGroup")
    for f in files["ClCompile"]:
        ET.SubElement(ig, "ClCompile", Include=f)
    ig = ET.SubElement(proj, "ItemGroup")
    for f in files["ClInclude"]:
        ET.SubElement(ig, "ClInclude", Include=f)

    # None items (natstepfilter etc.)
    if files["None"]:
        ig = ET.SubElement(proj, "ItemGroup")
        for f in files["None"]:
            ET.SubElement(ig, "None", Include=f)

    # Natvis items
    if files["Natvis"]:
        ig = ET.SubElement(proj, "ItemGroup")
        for f in files["Natvis"]:
            ET.SubElement(ig, "Natvis", Include=f)

    ET.SubElement(proj, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.targets")
    ET.SubElement(proj, "ImportGroup", Label="ExtensionTargets")

    write_xml(proj, ROOT / "Editor" / "Editor.vcxproj")


# ──────────────────────────────────────────────
# Client .vcxproj
# ──────────────────────────────────────────────
def generate_client_vcxproj(files: dict[str, list[str]]):
    proj = ET.Element("Project", DefaultTargets="Build", xmlns=NS)

    # ProjectConfigurations
    ig = ET.SubElement(proj, "ItemGroup", Label="ProjectConfigurations")
    for cfg, plat in CONFIGURATIONS:
        pc = ET.SubElement(ig, "ProjectConfiguration", Include=f"{cfg}|{plat}")
        ET.SubElement(pc, "Configuration").text = cfg
        ET.SubElement(pc, "Platform").text = plat

    # Globals
    pg = ET.SubElement(proj, "PropertyGroup", Label="Globals")
    ET.SubElement(pg, "VCProjectVersion").text = "17.0"
    ET.SubElement(pg, "Keyword").text = "Win32Proj"
    ET.SubElement(pg, "ProjectGuid").text = "{31536dd1-487b-419f-aa5a-1a6b996a7bef}"
    ET.SubElement(pg, "RootNamespace").text = "Client"
    ET.SubElement(pg, "WindowsTargetPlatformVersion").text = "10.0"

    ET.SubElement(proj, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props")

    # Configuration properties
    for cfg, plat in CONFIGURATIONS:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        pg = ET.SubElement(proj, "PropertyGroup", Condition=cond, Label="Configuration")
        is_release = cfg == "Release"
        ET.SubElement(pg, "ConfigurationType").text = "Application"
        ET.SubElement(pg, "UseDebugLibraries").text = "false" if is_release else "true"
        ET.SubElement(pg, "PlatformToolset").text = "v143"
        if is_release:
            ET.SubElement(pg, "WholeProgramOptimization").text = "true"
        ET.SubElement(pg, "CharacterSet").text = "Unicode"

    ET.SubElement(proj, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.props")
    ET.SubElement(proj, "ImportGroup", Label="ExtensionSettings")
    ET.SubElement(proj, "ImportGroup", Label="Shared")

    # PropertySheets
    for cfg, plat in CONFIGURATIONS:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        ig = ET.SubElement(proj, "ImportGroup", Label="PropertySheets", Condition=cond)
        ET.SubElement(ig, "Import",
                      Project="$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props",
                      Condition="exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')",
                      Label="LocalAppDataPlatform")

    ET.SubElement(proj, "PropertyGroup", Label="UserMacros")

    # Output/Intermediate dirs + DisableFastUpToDateCheck for x64
    for cfg in ("Debug", "Release"):
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|x64'"
        pg = ET.SubElement(proj, "PropertyGroup", Condition=cond)
        ET.SubElement(pg, "OutDir").text = "$(ProjectDir)Bin\\$(Configuration)\\"
        ET.SubElement(pg, "IntDir").text = "Build\\$(Configuration)\\"
        ET.SubElement(pg, "DisableFastUpToDateCheck").text = "true"

    # ItemDefinitionGroups
    for cfg, plat in CONFIGURATIONS:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        idg = ET.SubElement(proj, "ItemDefinitionGroup", Condition=cond)
        cl = ET.SubElement(idg, "ClCompile")
        ET.SubElement(cl, "WarningLevel").text = "Level3"

        is_x64 = plat == "x64"
        is_release = cfg == "Release"
        is_win32 = plat == "Win32"

        if is_release:
            ET.SubElement(cl, "FunctionLevelLinking").text = "true"
            ET.SubElement(cl, "IntrinsicFunctions").text = "true"

        ET.SubElement(cl, "SDLCheck").text = "true"

        if is_win32:
            defs = f"WIN32;{'NDEBUG' if is_release else '_DEBUG'};_CONSOLE;%(PreprocessorDefinitions)"
        else:
            defs = f"{'NDEBUG' if is_release else '_DEBUG'};_CONSOLE;%(PreprocessorDefinitions)"
        ET.SubElement(cl, "PreprocessorDefinitions").text = defs

        ET.SubElement(cl, "ConformanceMode").text = "true"

        if is_x64:
            ET.SubElement(cl, "LanguageStandard").text = "stdcpp20"
            ET.SubElement(cl, "AdditionalOptions").text = "/utf-8 %(AdditionalOptions)"
            ET.SubElement(cl, "AdditionalIncludeDirectories").text = (
                "$(ProjectDir)Source\\;$(ProjectDir)..\\Engine\\Source\\;%(AdditionalIncludeDirectories)"
            )

        link = ET.SubElement(idg, "Link")
        if is_x64:
            ET.SubElement(link, "SubSystem").text = "Windows"
            ET.SubElement(link, "GenerateDebugInformation").text = "true"
            ET.SubElement(link, "AdditionalLibraryDirectories").text = (
                "$(ProjectDir)..\\Engine\\Bin\\$(Configuration)\\;%(AdditionalLibraryDirectories)"
            )
            ET.SubElement(link, "AdditionalDependencies").text = "Engine.lib;%(AdditionalDependencies)"

            pbe = ET.SubElement(idg, "PostBuildEvent")
            ET.SubElement(pbe, "Command").text = (
                'xcopy /Y /D "$(ProjectDir)..\\Engine\\Bin\\$(Configuration)\\Engine.dll" "$(OutDir)"'
            )
        else:
            ET.SubElement(link, "SubSystem").text = "Console"
            ET.SubElement(link, "GenerateDebugInformation").text = "true"

    # File items
    ig = ET.SubElement(proj, "ItemGroup")
    for f in files["ClCompile"]:
        ET.SubElement(ig, "ClCompile", Include=f)
    if files["ClInclude"]:
        ig = ET.SubElement(proj, "ItemGroup")
        for f in files["ClInclude"]:
            ET.SubElement(ig, "ClInclude", Include=f)

    ET.SubElement(proj, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.targets")
    ET.SubElement(proj, "ImportGroup", Label="ExtensionTargets")

    write_xml(proj, ROOT / "Client" / "Client.vcxproj")


# ──────────────────────────────────────────────
# .vcxproj.filters
# ──────────────────────────────────────────────
def generate_filters(project_name: str, files: dict[str, list[str]]):
    proj = ET.Element("Project", ToolsVersion="4.0", xmlns=NS)

    # Collect all filter paths
    all_filters = collect_all_filters(files)

    if all_filters:
        ig = ET.SubElement(proj, "ItemGroup")
        for filt in sorted(all_filters):
            f_elem = ET.SubElement(ig, "Filter", Include=filt)
            # Generate a deterministic unique identifier from the filter path
            import hashlib
            h = hashlib.md5(f"{project_name}:{filt}".encode()).hexdigest()
            uid = f"{{{h[:8]}-{h[8:12]}-{h[12:16]}-{h[16:20]}-{h[20:32]}}}"
            ET.SubElement(f_elem, "UniqueIdentifier").text = uid

    # ClCompile items with filters
    if files["ClCompile"]:
        ig = ET.SubElement(proj, "ItemGroup")
        for f in files["ClCompile"]:
            filt = get_filter(f)
            elem = ET.SubElement(ig, "ClCompile", Include=f)
            if filt:
                ET.SubElement(elem, "Filter").text = filt

    # ClInclude items with filters
    if files["ClInclude"]:
        ig = ET.SubElement(proj, "ItemGroup")
        for f in files["ClInclude"]:
            filt = get_filter(f)
            elem = ET.SubElement(ig, "ClInclude", Include=f)
            if filt:
                ET.SubElement(elem, "Filter").text = filt

    # None items with filters
    if files["None"]:
        ig = ET.SubElement(proj, "ItemGroup")
        for f in files["None"]:
            filt = get_filter(f)
            elem = ET.SubElement(ig, "None", Include=f)
            if filt:
                ET.SubElement(elem, "Filter").text = filt

    # Natvis items with filters
    if files["Natvis"]:
        ig = ET.SubElement(proj, "ItemGroup")
        for f in files["Natvis"]:
            filt = get_filter(f)
            elem = ET.SubElement(ig, "Natvis", Include=f)
            if filt:
                ET.SubElement(elem, "Filter").text = filt

    write_xml(proj, ROOT / project_name / f"{project_name}.vcxproj.filters", bom=True)


# ──────────────────────────────────────────────
# .sln
# ──────────────────────────────────────────────
def generate_sln():
    lines = []
    lines.append("")
    lines.append("Microsoft Visual Studio Solution File, Format Version 12.00")
    lines.append("# Visual Studio Version 17")
    lines.append("VisualStudioVersion = 17.14.37012.4 d17.14")
    lines.append("MinimumVisualStudioVersion = 10.0.40219.1")

    for name, info in PROJECTS.items():
        guid = info["guid"]
        lines.append(
            f'Project("{VS_PROJECT_TYPE}") = "{name}", "{name}\\{name}.vcxproj", "{guid}"'
        )
        if info["dependencies"]:
            lines.append("\tProjectSection(ProjectDependencies) = postProject")
            for dep in info["dependencies"]:
                lines.append(f"\t\t{dep} = {dep}")
            lines.append("\tEndProjectSection")
        lines.append("EndProject")

    lines.append("Global")

    # SolutionConfigurationPlatforms
    lines.append("\tGlobalSection(SolutionConfigurationPlatforms) = preSolution")
    for cfg, plat in CONFIGURATIONS:
        sln_plat = "x86" if plat == "Win32" else plat
        lines.append(f"\t\t{cfg}|{sln_plat} = {cfg}|{sln_plat}")
    lines.append("\tEndGlobalSection")

    # ProjectConfigurationPlatforms
    lines.append("\tGlobalSection(ProjectConfigurationPlatforms) = postSolution")
    for name, info in PROJECTS.items():
        guid = info["guid"]
        for cfg, plat in CONFIGURATIONS:
            sln_plat = "x86" if plat == "Win32" else plat
            lines.append(f"\t\t{guid}.{cfg}|{sln_plat}.ActiveCfg = {cfg}|{plat}")
            lines.append(f"\t\t{guid}.{cfg}|{sln_plat}.Build.0 = {cfg}|{plat}")
    lines.append("\tEndGlobalSection")

    lines.append("\tGlobalSection(SolutionProperties) = preSolution")
    lines.append("\t\tHideSolutionNode = FALSE")
    lines.append("\tEndGlobalSection")

    lines.append("\tGlobalSection(ExtensibilityGlobals) = postSolution")
    lines.append(f"\t\tSolutionGuid = {SOLUTION_GUID}")
    lines.append("\tEndGlobalSection")

    lines.append("EndGlobal")
    lines.append("")

    sln_path = ROOT / "KraftonJungleEngine.sln"
    with open(sln_path, "w", encoding="utf-8-sig", newline="\r\n") as f:
        f.write("\n".join(lines))


# ──────────────────────────────────────────────
# Main
# ──────────────────────────────────────────────
def main():
    print("Scanning project files...")

    # Engine: Source/ + ThirdParty/
    engine_files = scan_files(ROOT / "Engine", ["Source", "ThirdParty"])
    # Also include packages.config as None
    engine_files["None"].append("packages.config")
    print(f"  Engine: {sum(len(v) for v in engine_files.values())} files")

    # Editor: Source/ + ThirdParty/
    editor_files = scan_files(ROOT / "Editor", ["Source", "ThirdParty"])
    print(f"  Editor: {sum(len(v) for v in editor_files.values())} files")

    # Client: Source/
    client_files = scan_files(ROOT / "Client", ["Source"])
    print(f"  Client: {sum(len(v) for v in client_files.values())} files")

    print("Generating project files...")

    generate_engine_vcxproj(engine_files)
    generate_filters("Engine", engine_files)
    print("  Engine.vcxproj + .filters")

    generate_editor_vcxproj(editor_files)
    generate_filters("Editor", editor_files)
    print("  Editor.vcxproj + .filters")

    generate_client_vcxproj(client_files)
    generate_filters("Client", client_files)
    print("  Client.vcxproj + .filters")

    generate_sln()
    print("  KraftonJungleEngine.sln")

    print("Done!")


if __name__ == "__main__":
    main()
