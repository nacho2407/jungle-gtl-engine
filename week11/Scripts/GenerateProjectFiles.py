"""
GenerateProjectFiles.py — Auto-generate .vcxproj, .vcxproj.filters
for W3_Jungle_Team6 from the on-disk folder structure.

Usage:
    python Scripts/GenerateProjectFiles.py
"""

import hashlib
import os
import xml.etree.ElementTree as ET
from pathlib import Path

# ──────────────────────────────────────────────
# Constants
# ──────────────────────────────────────────────
ROOT = Path(__file__).resolve().parent.parent

PROJECT_NAME = "W3_Jungle_Team6"
PROJECT_DIR = ROOT / PROJECT_NAME
PROJECT_GUID = "{55068e81-c0a0-49f9-ab7b-54aea968722b}"
ROOT_NAMESPACE = "Week2"

SOLUTION_GUID = "{4EBC5DD2-CECA-4722-9D19-87C7CB5F481B}"
VS_PROJECT_TYPE = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"

CONFIGURATIONS = [
    ("Debug", "Win32"),
    ("Release", "Win32"),
    ("Debug", "x64"),
    ("Release", "x64"),
]

# Directories to recursively scan for source files
SCAN_DIRS = ["Source", "ThirdParty"]

# Directories to scan for shader files
SHADER_DIRS = ["Shaders"]

# File extensions to include
SOURCE_EXTS = {".cpp", ".c", ".cc", ".cxx"}
HEADER_EXTS = {".h", ".hpp", ".hxx", ".inl"}
SHADER_EXTS = {".hlsl", ".hlsli"}
NATVIS_EXTS = {".natvis"}
NONE_EXTS = {".natstepfilter", ".config"}

# Root-level files to include (relative to project dir)
ROOT_FILES = ["main.cpp"]

# Include paths (relative to project dir)
INCLUDE_PATHS = [
    "Source\\Engine",
    "Source",
    "ThirdParty",
    "ThirdParty\\ImGui",
    "Source\\Editor",
    ".",
]

NS = "http://schemas.microsoft.com/developer/msbuild/2003"


# ──────────────────────────────────────────────
# Helpers
# ──────────────────────────────────────────────
def scan_files(project_dir: Path) -> dict[str, list[str]]:
    """Scan directories and collect files grouped by type."""
    result = {"ClCompile": [], "ClInclude": [], "FxCompile": [], "Natvis": [], "None": []}

    # Scan source/header directories
    for scan_dir in SCAN_DIRS:
        full_dir = project_dir / scan_dir
        if not full_dir.exists():
            continue
        for dirpath, _, filenames in os.walk(full_dir):
            for fname in sorted(filenames):
                full = Path(dirpath) / fname
                rel = full.relative_to(project_dir)
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

    # Scan shader directories
    for shader_dir in SHADER_DIRS:
        full_dir = project_dir / shader_dir
        if not full_dir.exists():
            continue
        for dirpath, _, filenames in os.walk(full_dir):
            for fname in sorted(filenames):
                full = Path(dirpath) / fname
                rel = full.relative_to(project_dir)
                rel_str = str(rel).replace("/", "\\")
                ext = full.suffix.lower()

                if ext in SHADER_EXTS:
                    result["FxCompile"].append(rel_str)

    # Add root-level files
    for root_file in ROOT_FILES:
        full = project_dir / root_file
        if full.exists():
            result["ClCompile"].append(root_file.replace("/", "\\"))

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
# .vcxproj
# ──────────────────────────────────────────────
def generate_vcxproj(files: dict[str, list[str]]):
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
    ET.SubElement(pg, "ProjectGuid").text = PROJECT_GUID
    ET.SubElement(pg, "RootNamespace").text = ROOT_NAMESPACE
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

    # OutDir, IntDir, IncludePath, WorkingDirectory for all configurations
    include_path_value = ";".join(INCLUDE_PATHS) + ";$(IncludePath)"
    for cfg, plat in CONFIGURATIONS:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        pg = ET.SubElement(proj, "PropertyGroup", Condition=cond)
        ET.SubElement(pg, "OutDir").text = f"$(ProjectDir)Bin\\$(Configuration)\\"
        ET.SubElement(pg, "IntDir").text = f"$(ProjectDir)Build\\$(Configuration)\\"
        ET.SubElement(pg, "IncludePath").text = include_path_value
        ET.SubElement(pg, "LocalDebuggerWorkingDirectory").text = "$(ProjectDir)"

    # ItemDefinitionGroups
    for cfg, plat in CONFIGURATIONS:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        idg = ET.SubElement(proj, "ItemDefinitionGroup", Condition=cond)
        cl = ET.SubElement(idg, "ClCompile")
        ET.SubElement(cl, "WarningLevel").text = "Level3"

        is_release = cfg == "Release"
        is_win32 = plat == "Win32"
        is_x64 = plat == "x64"

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
        ET.SubElement(cl, "AdditionalOptions").text = "/utf-8 %(AdditionalOptions)"

        if is_x64:
            ET.SubElement(cl, "LanguageStandard").text = "stdcpp17"

        link = ET.SubElement(idg, "Link")
        ET.SubElement(link, "SubSystem").text = "Windows" if is_x64 else "Console"
        ET.SubElement(link, "GenerateDebugInformation").text = "true"

    # ClCompile items
    ig = ET.SubElement(proj, "ItemGroup")
    for f in files["ClCompile"]:
        ET.SubElement(ig, "ClCompile", Include=f)

    # ClInclude items
    ig = ET.SubElement(proj, "ItemGroup")
    for f in files["ClInclude"]:
        ET.SubElement(ig, "ClInclude", Include=f)

    # FxCompile items (shaders)
    if files["FxCompile"]:
        ig = ET.SubElement(proj, "ItemGroup")
        for f in files["FxCompile"]:
            elem = ET.SubElement(ig, "FxCompile", Include=f)
            # Exclude shaders from build (compiled at runtime)
            for cfg, plat in CONFIGURATIONS:
                if plat == "x64":
                    cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
                    ET.SubElement(elem, "ExcludedFromBuild", Condition=cond).text = "true"

    # Natvis items
    if files["Natvis"]:
        ig = ET.SubElement(proj, "ItemGroup")
        for f in files["Natvis"]:
            ET.SubElement(ig, "Natvis", Include=f)

    # None items
    if files["None"]:
        ig = ET.SubElement(proj, "ItemGroup")
        for f in files["None"]:
            ET.SubElement(ig, "None", Include=f)

    ET.SubElement(proj, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.targets")
    ET.SubElement(proj, "ImportGroup", Label="ExtensionTargets")

    write_xml(proj, PROJECT_DIR / f"{PROJECT_NAME}.vcxproj")


# ──────────────────────────────────────────────
# .vcxproj.filters
# ──────────────────────────────────────────────
def generate_filters(files: dict[str, list[str]]):
    proj = ET.Element("Project", ToolsVersion="4.0", xmlns=NS)

    # Collect all filter paths
    all_filters = collect_all_filters(files)

    if all_filters:
        ig = ET.SubElement(proj, "ItemGroup")
        for filt in sorted(all_filters):
            f_elem = ET.SubElement(ig, "Filter", Include=filt)
            h = hashlib.md5(f"{PROJECT_NAME}:{filt}".encode()).hexdigest()
            uid = f"{{{h[:8]}-{h[8:12]}-{h[12:16]}-{h[16:20]}-{h[20:32]}}}"
            ET.SubElement(f_elem, "UniqueIdentifier").text = uid

    # FxCompile items with filters
    if files["FxCompile"]:
        ig = ET.SubElement(proj, "ItemGroup")
        for f in files["FxCompile"]:
            filt = get_filter(f)
            elem = ET.SubElement(ig, "FxCompile", Include=f)
            if filt:
                ET.SubElement(elem, "Filter").text = filt

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

    write_xml(proj, PROJECT_DIR / f"{PROJECT_NAME}.vcxproj.filters", bom=True)


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

    guid_upper = PROJECT_GUID.upper()
    lines.append(
        f'Project("{VS_PROJECT_TYPE}") = "{PROJECT_NAME}", '
        f'"{PROJECT_NAME}\\{PROJECT_NAME}.vcxproj", "{guid_upper}"'
    )
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
    for cfg, plat in CONFIGURATIONS:
        sln_plat = "x86" if plat == "Win32" else plat
        lines.append(f"\t\t{guid_upper}.{cfg}|{sln_plat}.ActiveCfg = {cfg}|{plat}")
        lines.append(f"\t\t{guid_upper}.{cfg}|{sln_plat}.Build.0 = {cfg}|{plat}")
    lines.append("\tEndGlobalSection")

    lines.append("\tGlobalSection(SolutionProperties) = preSolution")
    lines.append("\t\tHideSolutionNode = FALSE")
    lines.append("\tEndGlobalSection")

    lines.append("\tGlobalSection(ExtensibilityGlobals) = postSolution")
    lines.append(f"\t\tSolutionGuid = {SOLUTION_GUID}")
    lines.append("\tEndGlobalSection")

    lines.append("EndGlobal")
    lines.append("")

    sln_path = ROOT / f"{PROJECT_NAME}.sln"
    with open(sln_path, "w", encoding="utf-8-sig", newline="\r\n") as f:
        f.write("\n".join(lines))


# ──────────────────────────────────────────────
# Main
# ──────────────────────────────────────────────
def main():
    print(f"Scanning project files in {PROJECT_DIR}...")

    files = scan_files(PROJECT_DIR)

    print(f"  ClCompile:  {len(files['ClCompile'])} files")
    print(f"  ClInclude:  {len(files['ClInclude'])} files")
    print(f"  FxCompile:  {len(files['FxCompile'])} files")
    print(f"  Natvis:     {len(files['Natvis'])} files")
    print(f"  None:       {len(files['None'])} files")

    print("Generating project files...")

    generate_vcxproj(files)
    print(f"  {PROJECT_NAME}.vcxproj")

    generate_filters(files)
    print(f"  {PROJECT_NAME}.vcxproj.filters")

    generate_sln()
    print(f"  {PROJECT_NAME}.sln")

    print("Done!")


if __name__ == "__main__":
    main()
