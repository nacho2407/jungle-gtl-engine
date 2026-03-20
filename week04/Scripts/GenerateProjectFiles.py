"""
Regenerate the checked-in Visual Studio solution and VC++ project files
from the current on-disk project layout.

Usage:
    py Scripts/GenerateProjectFiles.py
"""

from __future__ import annotations

import hashlib
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOLUTION_NAME = "Kraftonjungle_Team2.sln"
SOLUTION_GUID = "{CC734427-CF8D-4087-B514-F2730B4E82A8}"
VS_PROJECT_TYPE = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"
WINDOWS_TARGET_PLATFORM_VERSION = "10.0"
NS = "http://schemas.microsoft.com/developer/msbuild/2003"

CONFIGURATIONS = [
    ("Debug", "x64"),
    ("Release", "x64"),
]
SOLUTION_CONFIGURATION_ORDER = [
    ("Debug", "x64"),
    ("Release", "x64"),
]
SOURCE_EXTS = {".c", ".cc", ".cpp", ".cxx"}
HEADER_EXTS = {".h", ".hpp", ".hxx", ".inl"}
NATVIS_EXTS = {".natvis"}
NONE_EXTS = {".config", ".natstepfilter"}

ENGINE_NUGET_ERROR_TEXT = (
    "\uc774 \ud504\ub85c\uc81d\ud2b8\ub294 \uc774 \ucef4\ud4e8\ud130\uc5d0 \uc5c6\ub294 NuGet \ud328\ud0a4\uc9c0\ub97c \ucc38\uc870\ud569\ub2c8\ub2e4. "
    "\ud574\ub2f9 \ud328\ud0a4\uc9c0\ub97c \ub2e4\uc6b4\ub85c\ub4dc\ud558\ub824\uba74 NuGet \ud328\ud0a4\uc9c0 \ubcf5\uc6d0\uc744 \uc0ac\uc6a9\ud558\uc2ed\uc2dc\uc624. "
    "\uc790\uc138\ud55c \ub0b4\uc6a9\uc740 http://go.microsoft.com/fwlink/?LinkID=322105\ub97c \ucc38\uc870\ud558\uc2ed\uc2dc\uc624. "
    "\ub204\ub77d\ub41c \ud30c\uc77c\uc740 {0}\uc785\ub2c8\ub2e4."
)




@dataclass(frozen=True)
class ProjectSpec:
    name: str
    guid: str
    root_namespace: str
    scan_roots: tuple[str, ...]
    extra_items: dict[str, tuple[str, ...]] = field(default_factory=dict)
    dependencies: tuple[str, ...] = ()


PROJECTS = (
    ProjectSpec(
        name="Engine",
        guid="{F44237E2-379F-4018-A189-153730228C6B}",
        root_namespace="Engine",
        scan_roots=("Source", "Resources"),
        extra_items={
            "ClCompile": ("main.cpp",),
            "None": ("packages.config",),
        },
    ),
    ProjectSpec(
        name="Editor",
        guid="{910D7C84-AF56-42A5-8FD5-77FAB20D77C6}",
        root_namespace="Editor",
        scan_roots=("Source",),
        dependencies=("{F44237E2-379F-4018-A189-153730228C6B}",),
    ),
)


def project_dir(spec: ProjectSpec) -> Path:
    return ROOT / spec.name


def to_windows_path(path: Path) -> str:
    return str(path).replace("/", "\\")


def classify_file(path: Path) -> str | None:
    extension = path.suffix.lower()
    if extension in SOURCE_EXTS:
        return "ClCompile"
    if extension in HEADER_EXTS:
        return "ClInclude"
    if extension in NATVIS_EXTS:
        return "Natvis"
    if extension in NONE_EXTS:
        return "None"
    return None


def sorted_unique(values: set[str]) -> list[str]:
    return sorted(values, key=lambda value: value.lower())


def scan_project_files(spec: ProjectSpec) -> dict[str, list[str]]:
    items = {
        "ClCompile": set(),
        "ClInclude": set(),
        "Natvis": set(),
        "None": set(),
    }

    root_dir = project_dir(spec)
    for scan_root in spec.scan_roots:
        absolute_root = root_dir / scan_root
        if not absolute_root.is_dir():
            continue

        for file_path in absolute_root.rglob("*"):
            if not file_path.is_file():
                continue

            item_type = classify_file(file_path)
            if item_type is None:
                continue

            items[item_type].add(to_windows_path(file_path.relative_to(root_dir)))

    for item_type, relative_paths in spec.extra_items.items():
        for relative_path in relative_paths:
            candidate = root_dir / relative_path
            if candidate.is_file():
                items[item_type].add(relative_path.replace("/", "\\"))

    return {item_type: sorted_unique(paths) for item_type, paths in items.items()}


def collect_filter_paths(spec: ProjectSpec) -> list[str]:
    filters: set[str] = set()
    root_dir = project_dir(spec)

    for scan_root in spec.scan_roots:
        absolute_root = root_dir / scan_root
        if not absolute_root.is_dir():
            continue

        filters.add(to_windows_path(absolute_root.relative_to(root_dir)))
        for directory in absolute_root.rglob("*"):
            if directory.is_dir():
                filters.add(to_windows_path(directory.relative_to(root_dir)))

    return sorted(filters, key=lambda value: (value.count("\\"), value.lower()))


def filter_for_path(relative_path: str) -> str:
    normalized = relative_path.replace("/", "\\")
    return normalized.rsplit("\\", 1)[0] if "\\" in normalized else ""


def indent_xml(element: ET.Element, level: int = 0) -> None:
    indent = "\n" + "  " * level
    if len(element):
        if not element.text or not element.text.strip():
            element.text = indent + "  "
        for child in element:
            indent_xml(child, level + 1)
            if not child.tail or not child.tail.strip():
                child.tail = indent + "  "
        if not element[-1].tail or not element[-1].tail.strip():
            element[-1].tail = indent
    elif level and (not element.tail or not element.tail.strip()):
        element.tail = indent


def write_xml(root_element: ET.Element, destination: Path, bom: bool = False) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    indent_xml(root_element)
    xml_body = ET.tostring(root_element, encoding="utf-8")
    xml_bytes = b'<?xml version="1.0" encoding="utf-8"?>\n' + xml_body
    if bom:
        xml_bytes = b"\xef\xbb\xbf" + xml_bytes
    destination.write_bytes(xml_bytes)


def add_empty_import_group(root: ET.Element, **attributes: str) -> ET.Element:
    element = ET.SubElement(root, "ImportGroup", **attributes)
    # Keep empty import groups stable without serializer-specific byte replacements.
    element.text = "\n  "
    return element


def filter_guid(project_name: str, filter_path: str) -> str:
    digest = hashlib.md5(f"{project_name}:{filter_path}".encode("utf-8")).hexdigest()
    return f"{{{digest[:8]}-{digest[8:12]}-{digest[12:16]}-{digest[16:20]}-{digest[20:32]}}}"


def add_project_configurations(root: ET.Element) -> None:
    item_group = ET.SubElement(root, "ItemGroup", Label="ProjectConfigurations")
    for configuration, platform in CONFIGURATIONS:
        project_configuration = ET.SubElement(
            item_group, "ProjectConfiguration", Include=f"{configuration}|{platform}"
        )
        ET.SubElement(project_configuration, "Configuration").text = configuration
        ET.SubElement(project_configuration, "Platform").text = platform


def add_import_groups(root: ET.Element) -> None:
    add_empty_import_group(root, Label="ExtensionSettings")
    add_empty_import_group(root, Label="Shared")

    for configuration, platform in CONFIGURATIONS:
        condition = f"'$(Configuration)|$(Platform)'=='{configuration}|{platform}'"
        import_group = ET.SubElement(
            root, "ImportGroup", Label="PropertySheets", Condition=condition
        )
        ET.SubElement(
            import_group,
            "Import",
            Project="$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props",
            Condition="exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')",
            Label="LocalAppDataPlatform",
        )


def add_x64_output_dirs(root: ET.Element) -> None:
    for configuration in ("Debug", "Release"):
        condition = f"'$(Configuration)|$(Platform)'=='{configuration}|x64'"
        property_group = ET.SubElement(root, "PropertyGroup", Condition=condition)
        ET.SubElement(property_group, "OutDir").text = "$(ProjectDir)Bin\\$(Configuration)\\"
        ET.SubElement(property_group, "IntDir").text = "Build\\$(Configuration)\\"


def add_engine_project(files: dict[str, list[str]]) -> None:
    root = ET.Element("Project", DefaultTargets="Build", xmlns=NS)
    add_project_configurations(root)

    include_group = ET.SubElement(root, "ItemGroup")
    for path in files["ClInclude"]:
        ET.SubElement(include_group, "ClInclude", Include=path)

    none_group = ET.SubElement(root, "ItemGroup")
    for path in files["None"]:
        ET.SubElement(none_group, "None", Include=path)

    compile_group = ET.SubElement(root, "ItemGroup")
    for path in files["ClCompile"]:
        ET.SubElement(compile_group, "ClCompile", Include=path)

    globals_group = ET.SubElement(root, "PropertyGroup", Label="Globals")
    ET.SubElement(globals_group, "VCProjectVersion").text = "17.0"
    ET.SubElement(globals_group, "Keyword").text = "Win32Proj"
    ET.SubElement(globals_group, "ProjectGuid").text = PROJECTS[0].guid.lower()
    ET.SubElement(globals_group, "RootNamespace").text = "Engine"
    ET.SubElement(globals_group, "WindowsTargetPlatformVersion").text = WINDOWS_TARGET_PLATFORM_VERSION

    ET.SubElement(root, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props")

    for configuration, platform in CONFIGURATIONS:
        condition = f"'$(Configuration)|$(Platform)'=='{configuration}|{platform}'"
        property_group = ET.SubElement(root, "PropertyGroup", Condition=condition, Label="Configuration")
        ET.SubElement(property_group, "ConfigurationType").text = (
            "DynamicLibrary" if platform == "x64" else "Application"
        )
        ET.SubElement(property_group, "UseDebugLibraries").text = (
            "false" if configuration == "Release" else "true"
        )
        ET.SubElement(property_group, "PlatformToolset").text = "v143"
        if configuration == "Release":
            ET.SubElement(property_group, "WholeProgramOptimization").text = "true"
        ET.SubElement(property_group, "CharacterSet").text = "Unicode"

    ET.SubElement(root, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.props")
    add_import_groups(root)
    ET.SubElement(root, "PropertyGroup", Label="UserMacros")
    add_x64_output_dirs(root)

    for configuration, platform in CONFIGURATIONS:
        condition = f"'$(Configuration)|$(Platform)'=='{configuration}|{platform}'"
        item_definition_group = ET.SubElement(root, "ItemDefinitionGroup", Condition=condition)
        cl_compile = ET.SubElement(item_definition_group, "ClCompile")
        ET.SubElement(cl_compile, "WarningLevel").text = "Level3"
        if configuration == "Release":
            ET.SubElement(cl_compile, "FunctionLevelLinking").text = "true"
            ET.SubElement(cl_compile, "IntrinsicFunctions").text = "true"
        ET.SubElement(cl_compile, "SDLCheck").text = "true"
        if platform == "Win32":
            preprocessor_definitions = (
                "WIN32;NDEBUG;_CONSOLE;%(PreprocessorDefinitions)"
                if configuration == "Release"
                else "WIN32;_DEBUG;_CONSOLE;%(PreprocessorDefinitions)"
            )
        else:
            preprocessor_definitions = (
                "ENGINECORE_EXPORTS;NOMINMAX;NDEBUG;_CONSOLE;%(PreprocessorDefinitions)"
                if configuration == "Release"
                else "ENGINECORE_EXPORTS;NOMINMAX;_DEBUG;_CONSOLE;%(PreprocessorDefinitions)"
            )
        ET.SubElement(cl_compile, "PreprocessorDefinitions").text = preprocessor_definitions
        ET.SubElement(cl_compile, "ConformanceMode").text = "true"
        if platform == "x64":
            ET.SubElement(cl_compile, "LanguageStandard").text = "stdcpp20"
            ET.SubElement(cl_compile, "AdditionalIncludeDirectories").text = (
                "$(ProjectDir)Source;%(AdditionalIncludeDirectories)"
            )
            ET.SubElement(cl_compile, "AdditionalOptions").text = "/utf-8 %(AdditionalOptions)"

        link = ET.SubElement(item_definition_group, "Link")
        ET.SubElement(link, "SubSystem").text = "Console"
        ET.SubElement(link, "GenerateDebugInformation").text = "true"

    ET.SubElement(root, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.targets")

    extension_targets = ET.SubElement(root, "ImportGroup", Label="ExtensionTargets")
    ET.SubElement(
        extension_targets,
        "Import",
        Project="..\\packages\\directxtk_desktop_win10.2025.10.28.2\\build\\native\\directxtk_desktop_win10.targets",
        Condition=(
            "Exists('..\\packages\\directxtk_desktop_win10.2025.10.28.2\\build\\native\\directxtk_desktop_win10.targets')"
        ),
    )

    target = ET.SubElement(
        root,
        "Target",
        Name="EnsureNuGetPackageBuildImports",
        BeforeTargets="PrepareForBuild",
    )
    property_group = ET.SubElement(target, "PropertyGroup")
    ET.SubElement(property_group, "ErrorText").text = ENGINE_NUGET_ERROR_TEXT
    ET.SubElement(
        target,
        "Error",
        Condition=(
            "!Exists('..\\packages\\directxtk_desktop_win10.2025.10.28.2\\build\\native\\directxtk_desktop_win10.targets')"
        ),
        Text=(
            "$([System.String]::Format('$(ErrorText)', "
            "'..\\packages\\directxtk_desktop_win10.2025.10.28.2\\build\\native\\directxtk_desktop_win10.targets'))"
        ),
    )

    write_xml(root, ROOT / "Engine" / "Engine.vcxproj")


def add_editor_project(files: dict[str, list[str]]) -> None:
    root = ET.Element("Project", DefaultTargets="Build", xmlns=NS)
    add_project_configurations(root)

    globals_group = ET.SubElement(root, "PropertyGroup", Label="Globals")
    ET.SubElement(globals_group, "VCProjectVersion").text = "17.0"
    ET.SubElement(globals_group, "Keyword").text = "Win32Proj"
    ET.SubElement(globals_group, "ProjectGuid").text = PROJECTS[1].guid.lower()
    ET.SubElement(globals_group, "RootNamespace").text = "Editor"
    ET.SubElement(globals_group, "WindowsTargetPlatformVersion").text = WINDOWS_TARGET_PLATFORM_VERSION

    ET.SubElement(root, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props")

    for configuration, platform in CONFIGURATIONS:
        condition = f"'$(Configuration)|$(Platform)'=='{configuration}|{platform}'"
        property_group = ET.SubElement(root, "PropertyGroup", Condition=condition, Label="Configuration")
        ET.SubElement(property_group, "ConfigurationType").text = "Application"
        ET.SubElement(property_group, "UseDebugLibraries").text = (
            "false" if configuration == "Release" else "true"
        )
        ET.SubElement(property_group, "PlatformToolset").text = "v143"
        if configuration == "Release":
            ET.SubElement(property_group, "WholeProgramOptimization").text = "true"
        ET.SubElement(property_group, "CharacterSet").text = "Unicode"

    ET.SubElement(root, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.props")
    add_import_groups(root)
    ET.SubElement(root, "PropertyGroup", Label="UserMacros")
    add_x64_output_dirs(root)

    for configuration, platform in CONFIGURATIONS:
        condition = f"'$(Configuration)|$(Platform)'=='{configuration}|{platform}'"
        item_definition_group = ET.SubElement(root, "ItemDefinitionGroup", Condition=condition)
        cl_compile = ET.SubElement(item_definition_group, "ClCompile")
        ET.SubElement(cl_compile, "WarningLevel").text = "Level3"
        if configuration == "Release":
            ET.SubElement(cl_compile, "FunctionLevelLinking").text = "true"
            ET.SubElement(cl_compile, "IntrinsicFunctions").text = "true"
        ET.SubElement(cl_compile, "SDLCheck").text = "true"
        if platform == "Win32":
            preprocessor_definitions = (
                "WIN32;NDEBUG;_CONSOLE;%(PreprocessorDefinitions)"
                if configuration == "Release"
                else "WIN32;_DEBUG;_CONSOLE;%(PreprocessorDefinitions)"
            )
        else:
            preprocessor_definitions = (
                "NOMINMAX;NDEBUG;_WINDOWS;%(PreprocessorDefinitions)"
                if configuration == "Release"
                else "NOMINMAX;_DEBUG;_WINDOWS;%(PreprocessorDefinitions)"
            )
        ET.SubElement(cl_compile, "PreprocessorDefinitions").text = preprocessor_definitions
        ET.SubElement(cl_compile, "ConformanceMode").text = "true"
        if platform == "x64":
            ET.SubElement(cl_compile, "LanguageStandard").text = "stdcpp20"
            ET.SubElement(cl_compile, "AdditionalIncludeDirectories").text = (
                "$(ProjectDir)Source;$(ProjectDir)Source\\ThirdParty\\imgui;$(ProjectDir)..\\Engine\\Source;%(AdditionalIncludeDirectories)"
            )
            ET.SubElement(cl_compile, "AdditionalOptions").text = "/utf-8 %(AdditionalOptions)"

        link = ET.SubElement(item_definition_group, "Link")
        ET.SubElement(link, "SubSystem").text = "Windows"
        ET.SubElement(link, "GenerateDebugInformation").text = "true"
        if platform == "x64":
            ET.SubElement(link, "AdditionalDependencies").text = "Engine.lib;%(AdditionalDependencies)"
            ET.SubElement(link, "AdditionalLibraryDirectories").text = (
                "$(ProjectDir)..\\Engine\\Bin\\$(Configuration);%(AdditionalLibraryDirectories)"
            )
            pre_build_event = ET.SubElement(item_definition_group, "PreBuildEvent")
            ET.SubElement(pre_build_event, "Command").text = (
                'if not exist "$(OutDir)" mkdir "$(OutDir)"\n'
                'if exist "$(ProjectDir)..\\Engine\\Bin\\$(Configuration)\\Engine.dll" '
                'copy /Y "$(ProjectDir)..\\Engine\\Bin\\$(Configuration)\\Engine.dll" "$(OutDir)"\n'
                'if exist "$(ProjectDir)..\\Engine\\Bin\\$(Configuration)\\Engine.pdb" '
                'copy /Y "$(ProjectDir)..\\Engine\\Bin\\$(Configuration)\\Engine.pdb" "$(OutDir)"'
            )
    compile_group = ET.SubElement(root, "ItemGroup")
    for path in files["ClCompile"]:
        ET.SubElement(compile_group, "ClCompile", Include=path)

    include_group = ET.SubElement(root, "ItemGroup")
    for path in files["ClInclude"]:
        ET.SubElement(include_group, "ClInclude", Include=path)

    if files["None"]:
        none_group = ET.SubElement(root, "ItemGroup")
        for path in files["None"]:
            ET.SubElement(none_group, "None", Include=path)

    if files["Natvis"]:
        natvis_group = ET.SubElement(root, "ItemGroup")
        for path in files["Natvis"]:
            ET.SubElement(natvis_group, "Natvis", Include=path)

    project_reference_group = ET.SubElement(root, "ItemGroup")
    project_reference = ET.SubElement(
        project_reference_group,
        "ProjectReference",
        Include="..\\Engine\\Engine.vcxproj",
    )
    ET.SubElement(project_reference, "Project").text = PROJECTS[0].guid
    ET.SubElement(project_reference, "ReferenceOutputAssembly").text = "false"

    ET.SubElement(root, "Import", Project="$(VCTargetsPath)\\Microsoft.Cpp.targets")
    add_empty_import_group(root, Label="ExtensionTargets")

    write_xml(root, ROOT / "Editor" / "Editor.vcxproj")


def generate_filters(spec: ProjectSpec, files: dict[str, list[str]]) -> None:
    root = ET.Element("Project", ToolsVersion="4.0", xmlns=NS)

    filter_paths = collect_filter_paths(spec)
    if filter_paths:
        filter_group = ET.SubElement(root, "ItemGroup")
        for filter_path in filter_paths:
            filter_element = ET.SubElement(filter_group, "Filter", Include=filter_path)
            ET.SubElement(filter_element, "UniqueIdentifier").text = filter_guid(spec.name, filter_path)

    for item_type in ("ClCompile", "ClInclude", "None", "Natvis"):
        if not files[item_type]:
            continue

        item_group = ET.SubElement(root, "ItemGroup")
        for relative_path in files[item_type]:
            item_element = ET.SubElement(item_group, item_type, Include=relative_path)
            item_filter = filter_for_path(relative_path)
            if item_filter:
                ET.SubElement(item_element, "Filter").text = item_filter

    write_xml(root, ROOT / spec.name / f"{spec.name}.vcxproj.filters", bom=True)


def generate_solution(projects: tuple[ProjectSpec, ...]) -> None:
    lines = [
        "",
        "Microsoft Visual Studio Solution File, Format Version 12.00",
        "# Visual Studio Version 17",
        "VisualStudioVersion = 17.14.37012.4 d17.14",
        "MinimumVisualStudioVersion = 10.0.40219.1",
    ]

    for project in projects:
        lines.append(
            f'Project("{VS_PROJECT_TYPE}") = "{project.name}", "{project.name}\\{project.name}.vcxproj", "{project.guid}"'
        )
        if project.dependencies:
            lines.append("\tProjectSection(ProjectDependencies) = postProject")
            for dependency in project.dependencies:
                lines.append(f"\t\t{dependency} = {dependency}")
            lines.append("\tEndProjectSection")
        lines.append("EndProject")

    lines.extend(
        [
            "Global",
            "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution",
        ]
    )
    for configuration, platform in SOLUTION_CONFIGURATION_ORDER:
        solution_platform = "x86" if platform == "Win32" else platform
        lines.append(f"\t\t{configuration}|{solution_platform} = {configuration}|{solution_platform}")
    lines.append("\tEndGlobalSection")

    lines.append("\tGlobalSection(ProjectConfigurationPlatforms) = postSolution")
    for project in projects:
        for configuration, platform in SOLUTION_CONFIGURATION_ORDER:
            solution_platform = "x86" if platform == "Win32" else platform
            lines.append(
                f"\t\t{project.guid}.{configuration}|{solution_platform}.ActiveCfg = {configuration}|{platform}"
            )
            lines.append(
                f"\t\t{project.guid}.{configuration}|{solution_platform}.Build.0 = {configuration}|{platform}"
            )
    lines.extend(
        [
            "\tEndGlobalSection",
            "\tGlobalSection(SolutionProperties) = preSolution",
            "\t\tHideSolutionNode = FALSE",
            "\tEndGlobalSection",
            "\tGlobalSection(ExtensibilityGlobals) = postSolution",
            f"\t\tSolutionGuid = {SOLUTION_GUID}",
            "\tEndGlobalSection",
            "EndGlobal",
            "",
        ]
    )

    solution_path = ROOT / SOLUTION_NAME
    solution_path.write_text("\n".join(lines), encoding="utf-8-sig", newline="\n")


def main() -> None:
    ET.register_namespace("", NS)

    print("Scanning project files...")
    scanned_files: dict[str, dict[str, list[str]]] = {}
    for spec in PROJECTS:
        files = scan_project_files(spec)
        scanned_files[spec.name] = files
        total = sum(len(entries) for entries in files.values())
        print(f"  {spec.name}: {total} files")

    print("Generating project files...")
    add_engine_project(scanned_files["Engine"])
    generate_filters(PROJECTS[0], scanned_files["Engine"])
    print("  Engine.vcxproj + .filters")

    add_editor_project(scanned_files["Editor"])
    generate_filters(PROJECTS[1], scanned_files["Editor"])
    print("  Editor.vcxproj + .filters")

    generate_solution(PROJECTS)
    print(f"  {SOLUTION_NAME}")
    print("Done!")


if __name__ == "__main__":
    main()