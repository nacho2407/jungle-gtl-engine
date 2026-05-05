#include "Editor/Packaging/GamePackager.h"

#include "Asset/BinarySerializer.h"
#include "Asset/ObjLoader.h"
#include "Asset/StaticMeshTypes.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "SimpleJSON/json.hpp"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <vector>

namespace
{
    constexpr const wchar_t* MSBuildPath =
        L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\amd64\\MSBuild.exe";

    void EmitBuildLog(const FString& Message);
    void EmitBuildLogWidePath(const char* Prefix, const std::filesystem::path& Path);

    struct FPackageCookContext
    {
        std::filesystem::path EngineRoot;
        std::filesystem::path OutputRoot;
        std::set<FString> FilesToCopy;
        std::set<FString> CopiedFiles;
        std::map<FString, FString> CookedMeshPathBySource;
        TArray<FString> ManifestLines;
    };

    const char* GetConfigurationName(EGameBuildConfiguration Configuration)
    {
        return Configuration == EGameBuildConfiguration::Shipping
            ? "GameClientRelease"
            : "GameClientDebug";
    }

    std::filesystem::path FindSolutionRoot()
    {
        std::vector<std::filesystem::path> SearchRoots;
        SearchRoots.emplace_back(std::filesystem::path(FPaths::RootDir()));
        SearchRoots.emplace_back(std::filesystem::current_path());

        WCHAR ModulePath[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, ModulePath, MAX_PATH) > 0)
        {
            SearchRoots.emplace_back(std::filesystem::path(ModulePath).parent_path());
        }

        for (std::filesystem::path Root : SearchRoots)
        {
            Root = Root.lexically_normal();
            while (!Root.empty())
            {
                if (std::filesystem::exists(Root / L"NipsEngine.sln"))
                {
                    return Root;
                }

                const std::filesystem::path Parent = Root.parent_path();
                if (Parent == Root)
                {
                    break;
                }
                Root = Parent;
            }
        }

        return {};
    }

    std::filesystem::path ResolveAgainstProjectRoot(const FString& Path)
    {
        std::filesystem::path Result(FPaths::ToWide(Path));
        if (Result.is_absolute())
        {
            return Result.lexically_normal();
        }
        const std::filesystem::path SolutionRoot = FindSolutionRoot();
        const std::filesystem::path Base = SolutionRoot.empty()
            ? std::filesystem::path(FPaths::RootDir())
            : SolutionRoot;
        return (Base / Result).lexically_normal();
    }

    std::filesystem::path ResolvePackageInputPath(const FString& Path)
    {
        std::filesystem::path Result(FPaths::ToWide(Path));
        if (Result.is_absolute())
        {
            return Result.lexically_normal();
        }

        const std::filesystem::path EngineRoot(FPaths::RootDir());
        const std::filesystem::path EngineRelative = (EngineRoot / Result).lexically_normal();
        if (std::filesystem::exists(EngineRelative))
        {
            return EngineRelative;
        }

        return ResolveAgainstProjectRoot(Path);
    }

    FString NormalizePackagePath(const std::filesystem::path& Path)
    {
        return FPaths::ToUtf8(Path.lexically_normal().generic_wstring());
    }

    FString EscapeRcPath(std::filesystem::path Path)
    {
        FString Result = FPaths::ToUtf8(Path.lexically_normal().wstring());
        for (size_t Index = 0; Index < Result.size(); ++Index)
        {
            if (Result[Index] == '\\')
            {
                Result.insert(Result.begin() + static_cast<std::ptrdiff_t>(Index), '\\');
                ++Index;
            }
            else if (Result[Index] == '"')
            {
                Result.insert(Result.begin() + static_cast<std::ptrdiff_t>(Index), '\\');
                ++Index;
            }
        }
        return Result;
    }

    FString ToLowerAscii(FString Value)
    {
        for (char& Ch : Value)
        {
            Ch = static_cast<char>(std::tolower(static_cast<unsigned char>(Ch)));
        }
        return Value;
    }

    bool HasExtension(const FString& Path, std::initializer_list<const char*> Extensions)
    {
        const FString Ext = ToLowerAscii(FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(Path)).extension().generic_wstring()));
        for (const char* Allowed : Extensions)
        {
            if (Ext == Allowed)
            {
                return true;
            }
        }
        return false;
    }

    FString GetLowerExtension(const FString& Path)
    {
        return ToLowerAscii(FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(Path)).extension().generic_wstring()));
    }

    bool ValidateOptionalBrandingFile(const FString& Path, std::initializer_list<const char*> Extensions, const char* Label, FString& OutMessage)
    {
        if (Path.empty())
        {
            return true;
        }

        if (!HasExtension(Path, Extensions))
        {
            OutMessage = FString(Label) + " has an unsupported extension: " + Path;
            EmitBuildLog(OutMessage);
            return false;
        }

        const std::filesystem::path ResolvedPath = ResolvePackageInputPath(Path);
        if (!std::filesystem::exists(ResolvedPath))
        {
            OutMessage = FString(Label) + " not found: " + Path;
            EmitBuildLog(OutMessage);
            return false;
        }

        return true;
    }

    FString NormalizeSceneForPackage(const FString& Scene)
    {
        std::filesystem::path ScenePath(FPaths::ToWide(Scene));
        if (ScenePath.is_absolute())
        {
            return FPaths::ToRelativeString(ScenePath.wstring());
        }
        return FPaths::Normalize(Scene);
    }

    FString NormalizeAssetPathForPackage(const FString& Path)
    {
        std::filesystem::path AssetPath(FPaths::ToWide(Path));
        if (AssetPath.is_absolute())
        {
            return FPaths::ToRelativeString(AssetPath.lexically_normal().wstring());
        }
        return FPaths::Normalize(Path);
    }

    std::filesystem::path ResolveProjectFilePath(const FString& Path)
    {
        std::filesystem::path Candidate(FPaths::ToWide(Path));
        if (Candidate.is_absolute())
        {
            return Candidate.lexically_normal();
        }
        return (std::filesystem::path(FPaths::RootDir()) / Candidate).lexically_normal();
    }

    bool FileExistsForPackage(const FString& Path)
    {
        const std::filesystem::path Resolved = ResolveProjectFilePath(Path);
        return std::filesystem::exists(Resolved) && std::filesystem::is_regular_file(Resolved);
    }

    bool IsRuntimeCopyExtension(const FString& Extension)
    {
        return Extension == ".bin"
            || Extension == ".mat"
            || Extension == ".matinst"
            || Extension == ".png"
            || Extension == ".jpg"
            || Extension == ".jpeg"
            || Extension == ".dds"
            || Extension == ".bmp"
            || Extension == ".wav"
            || Extension == ".mp3"
            || Extension == ".ogg"
            || Extension == ".lua"
            || Extension == ".prefab"
            || Extension == ".rml"
            || Extension == ".rcss"
            || Extension == ".meta"
            || Extension == ".ttf"
            || Extension == ".otf";
    }

    FString MakeCookedMeshRelativePath(const FString& SourceRelativePath)
    {
        std::filesystem::path SourcePath(FPaths::ToWide(SourceRelativePath));
        std::filesystem::path SubPath = SourcePath.filename();
        const std::filesystem::path AssetMeshRoot = std::filesystem::path(L"Asset") / L"Mesh";
        const std::filesystem::path RelativeToMesh = SourcePath.lexically_normal().lexically_relative(AssetMeshRoot);
        if (!RelativeToMesh.empty())
        {
            bool bHasParentReference = false;
            for (const std::filesystem::path& Part : RelativeToMesh)
            {
                if (Part == L"..")
                {
                    bHasParentReference = true;
                    break;
                }
            }
            if (!bHasParentReference)
            {
                SubPath = RelativeToMesh;
            }
        }

        SubPath.replace_extension(L".bin");
        return FPaths::ToUtf8((std::filesystem::path(L"Asset") / L"Cooked" / L"Mesh" / SubPath).generic_wstring());
    }

    FString MakeStaticMeshCacheBinaryPath(const FString& SourceRelativePath)
    {
        const std::filesystem::path SourcePath(FPaths::ToWide(FPaths::Normalize(SourceRelativePath)));
        std::filesystem::path BinaryFileName = SourcePath.stem();
        BinaryFileName += L".bin";
        return FPaths::ToUtf8((std::filesystem::path(L"Asset") / L"Mesh" / L"Bin" / BinaryFileName).generic_wstring());
    }

    uint64 GetPackageFileWriteTimeTicks(const std::filesystem::path& Path)
    {
        std::error_code Ec;
        const std::filesystem::file_time_type WriteTime = std::filesystem::last_write_time(Path, Ec);
        if (Ec)
        {
            return 0;
        }

        const auto Duration = WriteTime.time_since_epoch();
        return static_cast<uint64>(std::chrono::duration_cast<std::chrono::seconds>(Duration).count());
    }

    void AddManifest(FPackageCookContext& Context, const FString& Line);

    bool TryCopyCachedStaticMeshBinary(
        FPackageCookContext& Context,
        const FString& NormalizedSource,
        const std::filesystem::path& SourceAbs,
        const FString& CookedRelativePath,
        const std::filesystem::path& CookedAbs,
        FString& OutMessage)
    {
        const FString CacheRelativePath = MakeStaticMeshCacheBinaryPath(NormalizedSource);
        const std::filesystem::path CacheAbs = ResolveProjectFilePath(CacheRelativePath);
        std::error_code Ec;
        if (!std::filesystem::exists(CacheAbs, Ec) || !std::filesystem::is_regular_file(CacheAbs, Ec))
        {
            return false;
        }

        FBinarySerializer Serializer;
        FStaticMeshBinaryHeader Header;
        if (!Serializer.ReadStaticMeshHeader(FPaths::ToUtf8(CacheAbs.wstring()), Header))
        {
            return false;
        }

        const uint64 SourceWriteTime = GetPackageFileWriteTimeTicks(SourceAbs);
        if (SourceWriteTime == 0 || Header.SourceFileWriteTime != SourceWriteTime)
        {
            return false;
        }

        std::filesystem::create_directories(CookedAbs.parent_path(), Ec);
        if (Ec)
        {
            OutMessage = "Failed to create cooked mesh directory: " + CookedRelativePath;
            return false;
        }

        std::filesystem::copy_file(CacheAbs, CookedAbs, std::filesystem::copy_options::overwrite_existing, Ec);
        if (Ec)
        {
            OutMessage = "Failed to copy cached mesh: " + CacheRelativePath;
            return false;
        }

        Context.CookedMeshPathBySource[NormalizedSource] = CookedRelativePath;
        AddManifest(Context, "Copied cached mesh: " + NormalizedSource + " -> " + CookedRelativePath);
        return true;
    }

    void AddManifest(FPackageCookContext& Context, const FString& Line)
    {
        Context.ManifestLines.push_back(Line);
        EmitBuildLog(Line);
    }

    bool ResolveMaterialNameToFiles(const FString& MaterialName, TArray<FString>& OutMaterialFiles)
    {
        if (MaterialName.empty() || MaterialName.find('/') != FString::npos || MaterialName.find('\\') != FString::npos)
        {
            return false;
        }

        const std::filesystem::path MaterialRoot = std::filesystem::path(FPaths::RootDir()) / L"Asset" / L"Material";
        if (!std::filesystem::exists(MaterialRoot))
        {
            return false;
        }

        const FString LowerName = ToLowerAscii(MaterialName);
        for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(MaterialRoot))
        {
            if (!Entry.is_regular_file())
            {
                continue;
            }

            const FString Extension = ToLowerAscii(FPaths::ToUtf8(Entry.path().extension().generic_wstring()));
            if (Extension != ".mat" && Extension != ".matinst")
            {
                continue;
            }

            const FString Stem = FPaths::ToUtf8(Entry.path().stem().generic_wstring());
            const FString LowerStem = ToLowerAscii(Stem);
            if (LowerStem == LowerName || LowerStem.ends_with("_" + LowerName))
            {
                OutMaterialFiles.push_back(FPaths::ToRelativeString(Entry.path().wstring()));
            }
        }

        return !OutMaterialFiles.empty();
    }

    bool AddFileDependency(FPackageCookContext& Context, const FString& Path, FString& OutMessage);
    std::filesystem::path ResolveStartupSceneForValidation(const FString& StartupScene);

    void CollectAssetStringsFromTextFile(FPackageCookContext& Context, const std::filesystem::path& SourceFile)
    {
        std::ifstream In(SourceFile, std::ios::binary);
        if (!In.is_open())
        {
            return;
        }

        const FString Text((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
        constexpr const char* Prefixes[] = { "Asset/", "LuaScript/", "Shaders/" };
        for (const char* Prefix : Prefixes)
        {
            size_t SearchPos = 0;
            while ((SearchPos = Text.find(Prefix, SearchPos)) != FString::npos)
            {
                size_t EndPos = SearchPos;
                while (EndPos < Text.size())
                {
                    const char Ch = Text[EndPos];
                    if (Ch == '"' || Ch == '\'' || Ch == ')' || Ch == ',' || Ch == ';' || std::isspace(static_cast<unsigned char>(Ch)))
                    {
                        break;
                    }
                    ++EndPos;
                }

                if (EndPos > SearchPos)
                {
                    FString Candidate = Text.substr(SearchPos, EndPos - SearchPos);
                    FString IgnoredMessage;
                    AddFileDependency(Context, Candidate, IgnoredMessage);
                }
                SearchPos = EndPos;
            }
        }
    }

    bool CookStaticMeshDependency(FPackageCookContext& Context, const FString& SourcePath, FString& OutCookedPath, FString& OutMessage)
    {
        const FString NormalizedSource = NormalizeAssetPathForPackage(SourcePath);
        const FString Extension = GetLowerExtension(NormalizedSource);
        if (Extension == ".bin")
        {
            if (!FileExistsForPackage(NormalizedSource))
            {
                OutMessage = "Static mesh binary not found: " + NormalizedSource;
                return false;
            }
            Context.FilesToCopy.insert(NormalizedSource);
            OutCookedPath = NormalizedSource;
            return true;
        }

        if (Extension != ".obj")
        {
            OutMessage = "Unsupported static mesh source: " + NormalizedSource;
            return false;
        }

        auto Found = Context.CookedMeshPathBySource.find(NormalizedSource);
        if (Found != Context.CookedMeshPathBySource.end())
        {
            OutCookedPath = Found->second;
            return true;
        }

        const std::filesystem::path SourceAbs = ResolveProjectFilePath(NormalizedSource);
        if (!std::filesystem::exists(SourceAbs) || !std::filesystem::is_regular_file(SourceAbs))
        {
            OutMessage = "Static mesh source not found: " + NormalizedSource;
            return false;
        }

        const FString CookedRelativePath = MakeCookedMeshRelativePath(NormalizedSource);
        const std::filesystem::path CookedAbs = (Context.OutputRoot / std::filesystem::path(FPaths::ToWide(CookedRelativePath))).lexically_normal();
        if (TryCopyCachedStaticMeshBinary(Context, NormalizedSource, SourceAbs, CookedRelativePath, CookedAbs, OutMessage))
        {
            OutCookedPath = CookedRelativePath;
            return true;
        }
        if (!OutMessage.empty())
        {
            return false;
        }

        std::error_code Ec;
        std::filesystem::create_directories(CookedAbs.parent_path(), Ec);
        if (Ec)
        {
            OutMessage = "Failed to create cooked mesh directory: " + CookedRelativePath;
            return false;
        }

        FObjLoader ObjLoader;
        FStaticMeshLoadOptions LoadOptions = {};
        FStaticMesh* MeshData = ObjLoader.Load(FPaths::ToUtf8(SourceAbs.wstring()), LoadOptions);
        if (!MeshData)
        {
            OutMessage = "Failed to cook static mesh: " + NormalizedSource;
            return false;
        }

        FBinarySerializer Serializer;
        const bool bSaved = Serializer.SaveStaticMesh(
            FPaths::ToUtf8(CookedAbs.wstring()),
            FPaths::ToUtf8(SourceAbs.wstring()),
            *MeshData);
        delete MeshData;

        if (!bSaved)
        {
            OutMessage = "Failed to write cooked mesh: " + CookedRelativePath;
            return false;
        }

        Context.CookedMeshPathBySource[NormalizedSource] = CookedRelativePath;
        OutCookedPath = CookedRelativePath;
        AddManifest(Context, "Cooked mesh: " + NormalizedSource + " -> " + CookedRelativePath);
        return true;
    }

    bool CollectJsonDependenciesAndRewrite(json::JSON& Node, FPackageCookContext& Context, bool bInMaterialArray, FString& OutMessage)
    {
        switch (Node.JSONType())
        {
        case json::JSON::Class::String:
        {
            FString Value = Node.ToString();
            if (Value.empty())
            {
                return true;
            }

            if (bInMaterialArray)
            {
                TArray<FString> MaterialFiles;
                if (ResolveMaterialNameToFiles(Value, MaterialFiles))
                {
                    for (const FString& MaterialFile : MaterialFiles)
                    {
                        if (!AddFileDependency(Context, MaterialFile, OutMessage))
                        {
                            return false;
                        }
                    }
                }
            }

            const FString Extension = GetLowerExtension(Value);
            if (Extension == ".obj" || Extension == ".bin")
            {
                FString CookedPath;
                if (!CookStaticMeshDependency(Context, Value, CookedPath, OutMessage))
                {
                    return false;
                }
                Node = CookedPath;
                return true;
            }

            return AddFileDependency(Context, Value, OutMessage);
        }
        case json::JSON::Class::Object:
        {
            for (auto& Pair : Node.ObjectRange())
            {
                const bool bChildInMaterialArray = bInMaterialArray || Pair.first == "Materials";
                if (!CollectJsonDependenciesAndRewrite(Pair.second, Context, bChildInMaterialArray, OutMessage))
                {
                    return false;
                }
            }
            return true;
        }
        case json::JSON::Class::Array:
        {
            for (auto& Child : Node.ArrayRange())
            {
                if (!CollectJsonDependenciesAndRewrite(Child, Context, bInMaterialArray, OutMessage))
                {
                    return false;
                }
            }
            return true;
        }
        default:
            return true;
        }
    }

    bool AddFileDependency(FPackageCookContext& Context, const FString& Path, FString& OutMessage)
    {
        if (Path.empty())
        {
            return true;
        }

        const FString NormalizedPath = NormalizeAssetPathForPackage(Path);
        const FString Extension = GetLowerExtension(NormalizedPath);

        if (Extension == ".obj" || Extension == ".bin")
        {
            FString IgnoredCookedPath;
            return CookStaticMeshDependency(Context, NormalizedPath, IgnoredCookedPath, OutMessage);
        }

        if (Extension.empty() || Extension == ".scene" || !IsRuntimeCopyExtension(Extension))
        {
            return true;
        }

        if (NormalizedPath.rfind("Shaders/", 0) == 0)
        {
            return true;
        }

        if (!FileExistsForPackage(NormalizedPath))
        {
            return true;
        }

        const bool bInserted = Context.FilesToCopy.insert(NormalizedPath).second;
        if (!bInserted)
        {
            return true;
        }

        const std::filesystem::path SourceAbs = ResolveProjectFilePath(NormalizedPath);
        if (Extension == ".mat" || Extension == ".matinst" || Extension == ".prefab")
        {
            std::ifstream In(SourceAbs);
            if (In.is_open())
            {
                FString JsonText((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());
                json::JSON Root = json::JSON::Load(JsonText);
                if (Root.JSONType() == json::JSON::Class::Object || Root.JSONType() == json::JSON::Class::Array)
                {
                    if (!CollectJsonDependenciesAndRewrite(Root, Context, false, OutMessage))
                    {
                        return false;
                    }
                }
            }
        }
        else if (Extension == ".lua")
        {
            CollectAssetStringsFromTextFile(Context, SourceAbs);
        }

        return true;
    }

    bool AddKnownRuntimeDependencies(FPackageCookContext& Context, FString& OutMessage)
    {
        constexpr const char* Dependencies[] =
        {
            "Asset/Material/DecalMat.mat",
            "Asset/Mesh/Dice/Dice.obj"
        };

        for (const char* Dependency : Dependencies)
        {
            if (!AddFileDependency(Context, Dependency, OutMessage))
            {
                OutMessage = "Failed to include runtime dependency: " + FString(Dependency) + " | " + OutMessage;
                return false;
            }
        }

        return true;
    }

    bool AddPrefabDependencies(FPackageCookContext& Context, FString& OutMessage)
    {
        const std::filesystem::path PrefabRoot = Context.EngineRoot / L"Asset" / L"Prefab";
        std::error_code Ec;
        if (!std::filesystem::exists(PrefabRoot, Ec))
        {
            return true;
        }

        for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(PrefabRoot, Ec))
        {
            if (Ec)
            {
                OutMessage = "Failed to scan prefab dependencies";
                return false;
            }
            if (!Entry.is_regular_file())
            {
                continue;
            }

            const FString Extension = ToLowerAscii(FPaths::ToUtf8(Entry.path().extension().generic_wstring()));
            if (Extension != ".prefab")
            {
                continue;
            }

            const FString PrefabPath = FPaths::ToRelativeString(Entry.path().wstring());
            if (!AddFileDependency(Context, PrefabPath, OutMessage))
            {
                OutMessage = "Failed to include prefab dependency: " + PrefabPath + " | " + OutMessage;
                return false;
            }
        }

        return true;
    }

    bool WriteCookedScene(FPackageCookContext& Context, const FString& Scene, FString& OutMessage)
    {
        const std::filesystem::path SourceScene = ResolveStartupSceneForValidation(Scene);
        std::ifstream SceneFile(SourceScene);
        if (!SceneFile.is_open())
        {
            OutMessage = "Scene not found for cooking: " + Scene;
            return false;
        }

        FString SceneJson((std::istreambuf_iterator<char>(SceneFile)), std::istreambuf_iterator<char>());
        json::JSON Root = json::JSON::Load(SceneJson);
        if (!CollectJsonDependenciesAndRewrite(Root, Context, false, OutMessage))
        {
            return false;
        }

        const std::filesystem::path RelativeScene(FPaths::ToWide(NormalizeSceneForPackage(Scene)));
        const std::filesystem::path DestScene = (Context.OutputRoot / RelativeScene).lexically_normal();
        std::error_code Ec;
        std::filesystem::create_directories(DestScene.parent_path(), Ec);
        if (Ec)
        {
            OutMessage = "Failed to create cooked scene directory";
            return false;
        }

        std::ofstream OutScene(DestScene, std::ios::trunc);
        if (!OutScene.is_open())
        {
            OutMessage = "Failed to write cooked scene: " + NormalizeSceneForPackage(Scene);
            return false;
        }

        OutScene << Root.dump(1, "  ");
        AddManifest(Context, "Cooked scene: " + NormalizeSceneForPackage(Scene));
        return true;
    }

    bool CopyDependencyFiles(FPackageCookContext& Context, FString& OutMessage)
    {
        std::error_code Ec;
        for (const FString& RelativeFile : Context.FilesToCopy)
        {
            if (Context.CopiedFiles.find(RelativeFile) != Context.CopiedFiles.end())
            {
                continue;
            }

            const std::filesystem::path Source = ResolveProjectFilePath(RelativeFile);
            const std::filesystem::path Dest = (Context.OutputRoot / std::filesystem::path(FPaths::ToWide(RelativeFile))).lexically_normal();
            if (!std::filesystem::exists(Source, Ec))
            {
                continue;
            }

            std::filesystem::create_directories(Dest.parent_path(), Ec);
            if (Ec)
            {
                OutMessage = "Failed to create asset package directory: " + RelativeFile;
                return false;
            }

            std::filesystem::copy_file(Source, Dest, std::filesystem::copy_options::overwrite_existing, Ec);
            if (Ec)
            {
                OutMessage = "Failed to copy asset: " + RelativeFile;
                return false;
            }

            Context.CopiedFiles.insert(RelativeFile);
            AddManifest(Context, "Copied asset: " + RelativeFile);
        }
        return true;
    }

    bool CookAndCopyPackageAssets(const FGameBuildSettings& Settings, const std::filesystem::path& OutputRoot, const TArray<FString>& IncludedScenes, FString& OutMessage)
    {
        FPackageCookContext Context;
        Context.EngineRoot = std::filesystem::path(FPaths::RootDir()).lexically_normal();
        Context.OutputRoot = OutputRoot.lexically_normal();

        AddManifest(Context, "Packaging cook started");
        for (const FString& Scene : IncludedScenes)
        {
            if (!WriteCookedScene(Context, Scene, OutMessage))
            {
                return false;
            }
        }
        if (!AddKnownRuntimeDependencies(Context, OutMessage))
        {
            return false;
        }
        if (!AddPrefabDependencies(Context, OutMessage))
        {
            return false;
        }

        if (!CopyDependencyFiles(Context, OutMessage))
        {
            return false;
        }

        const std::filesystem::path ManifestPath = Context.OutputRoot / L"Settings" / L"PackageManifest.txt";
        std::error_code Ec;
        std::filesystem::create_directories(ManifestPath.parent_path(), Ec);
        if (!Ec)
        {
            std::ofstream Manifest(ManifestPath, std::ios::trunc);
            for (const FString& Line : Context.ManifestLines)
            {
                Manifest << Line << "\n";
            }
            Manifest << "CopiedAssetCount=" << Context.CopiedFiles.size() << "\n";
            Manifest << "CookedMeshCount=" << Context.CookedMeshPathBySource.size() << "\n";
            Manifest << "IncludedSceneCount=" << IncludedScenes.size() << "\n";
            Manifest << "StartupScene=" << NormalizeSceneForPackage(Settings.StartupScene) << "\n";
        }

        UE_LOG("[Build] Cook summary: Scenes=%d, Assets=%zu, Meshes=%zu",
            static_cast<int32>(IncludedScenes.size()),
            Context.CopiedFiles.size(),
            Context.CookedMeshPathBySource.size());
        return true;
    }

    TArray<FString> BuildIncludedSceneList(const FGameBuildSettings& Settings)
    {
        TArray<FString> Scenes;
        const auto AddUnique = [&Scenes](const FString& Scene)
        {
            const FString Normalized = NormalizeSceneForPackage(Scene);
            if (Normalized.empty())
            {
                return;
            }
            for (const FString& Existing : Scenes)
            {
                if (FPaths::Normalize(Existing) == Normalized)
                {
                    return;
                }
            }
            Scenes.push_back(Normalized);
        };

        AddUnique(Settings.StartupScene);
        for (const FString& Scene : Settings.IncludedScenes)
        {
            AddUnique(Scene);
        }
        return Scenes;
    }

    bool CopyDirectoryIfExists(const std::filesystem::path& Source, const std::filesystem::path& Dest, FString& OutMessage)
    {
        std::error_code Ec;
        if (!std::filesystem::exists(Source, Ec))
        {
            return true;
        }

        std::filesystem::create_directories(Dest, Ec);
        if (Ec)
        {
            OutMessage = "Failed to create package directory: " + FPaths::ToUtf8(Dest.wstring());
            return false;
        }

        std::filesystem::copy(
            Source,
            Dest,
            std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
            Ec);
        if (Ec)
        {
            OutMessage = "Failed to copy package files: " + FPaths::ToUtf8(Source.wstring());
            return false;
        }
        return true;
    }

    std::filesystem::path ResolveStartupSceneForValidation(const FString& StartupScene)
    {
        std::filesystem::path ScenePath(FPaths::ToWide(StartupScene));
        if (ScenePath.is_absolute())
        {
            return ScenePath.lexically_normal();
        }
        return std::filesystem::path(FPaths::ToAbsolute(FPaths::ToWide(StartupScene))).lexically_normal();
    }

    void EmitBuildLog(const FString& Message)
    {
        UE_LOG("[Build] %s", Message.c_str());
    }

    void EmitBuildLogWidePath(const char* Prefix, const std::filesystem::path& Path)
    {
        UE_LOG("[Build] %s%s", Prefix, FPaths::ToUtf8(Path.wstring()).c_str());
    }

    void FlushProcessOutput(FString& Buffer, bool bFlushPartialLine)
    {
        size_t Start = 0;
        while (Start < Buffer.size())
        {
            const size_t End = Buffer.find_first_of("\r\n", Start);
            if (End == FString::npos)
            {
                break;
            }

            FString Line = Buffer.substr(Start, End - Start);
            if (!Line.empty())
            {
                EmitBuildLog(Line);
            }

            Start = End + 1;
            while (Start < Buffer.size() && (Buffer[Start] == '\r' || Buffer[Start] == '\n'))
            {
                ++Start;
            }
        }

        Buffer.erase(0, Start);
        if (bFlushPartialLine && !Buffer.empty())
        {
            EmitBuildLog(Buffer);
            Buffer.clear();
        }
    }

    bool ReadAvailablePipeOutput(HANDLE ReadPipe, FString& PendingOutput)
    {
        DWORD AvailableBytes = 0;
        if (!PeekNamedPipe(ReadPipe, nullptr, 0, nullptr, &AvailableBytes, nullptr))
        {
            return false;
        }

        if (AvailableBytes == 0)
        {
            return true;
        }

        std::vector<char> Buffer(std::min<DWORD>(AvailableBytes, 4096));
        DWORD BytesRead = 0;
        if (!ReadFile(ReadPipe, Buffer.data(), static_cast<DWORD>(Buffer.size()), &BytesRead, nullptr) || BytesRead == 0)
        {
            return false;
        }

        PendingOutput.append(Buffer.data(), BytesRead);
        FlushProcessOutput(PendingOutput, false);
        return true;
    }

    bool ValidateStartupSceneForPackaging(const FGameBuildSettings& Settings, FString& OutMessage)
    {
        const std::filesystem::path ScenePath = ResolveStartupSceneForValidation(Settings.StartupScene);
        EmitBuildLogWidePath("Validating startup scene = ", ScenePath);

        std::ifstream SceneFile(ScenePath);
        if (!SceneFile.is_open())
        {
            OutMessage = "Startup scene not found: " + FPaths::ToUtf8(ScenePath.wstring());
            EmitBuildLog(OutMessage);
            return false;
        }

        FString SceneJson((std::istreambuf_iterator<char>(SceneFile)), std::istreambuf_iterator<char>());
        json::JSON Root = json::JSON::Load(SceneJson);
        if (!Root.hasKey("Actors"))
        {
            OutMessage = "Startup scene is invalid: Actors section missing";
            EmitBuildLog(OutMessage);
            return false;
        }

        int32 PlayerStartCount = 0;
        json::JSON& Actors = Root["Actors"];
        for (int32 Index = 0; Index < static_cast<int32>(Actors.length()); ++Index)
        {
            json::JSON& Data = Actors.at(Index);
            if (Data.hasKey("ClassName") && Data["ClassName"].ToString() == "APlayerStart")
            {
                ++PlayerStartCount;
            }
        }

        if (PlayerStartCount != 1)
        {
            OutMessage = "Packaging validation failed: Startup scene must contain exactly one Player Start.";
            UE_LOG_ERROR("[Build] %s Found=%d", OutMessage.c_str(), PlayerStartCount);
            EmitBuildLog("Add a Player Start to the startup scene, save it, then build again.");
            return false;
        }

        EmitBuildLog("Startup scene validation passed");
        return true;
    }

    bool ValidateIncludedScenesForPackaging(const FGameBuildSettings& Settings, FString& OutMessage)
    {
        const TArray<FString> Scenes = BuildIncludedSceneList(Settings);
        UE_LOG("[Build] Validating scene copy list. Count=%d", static_cast<int32>(Scenes.size()));
        for (const FString& Scene : Scenes)
        {
            const std::filesystem::path ScenePath = ResolveStartupSceneForValidation(Scene);
            if (!std::filesystem::exists(ScenePath))
            {
                OutMessage = "Scene to copy not found: " + Scene;
                EmitBuildLog(OutMessage);
                return false;
            }
        }
        return true;
    }
}

FGamePackageResult FGamePackager::BuildAndPackage(const FGameBuildSettings& Settings)
{
    FGamePackageResult Result;
    Result.OutputDirectory = FPaths::ToUtf8(ResolveAgainstProjectRoot(Settings.OutputDirectory).wstring());

    FString Message;
    if (!ValidateStartupSceneForPackaging(Settings, Message))
    {
        Result.Message = Message;
        UE_LOG_ERROR("[Build] Build Failed: %s", Message.c_str());
        return Result;
    }

    if (!ValidateIncludedScenesForPackaging(Settings, Message))
    {
        Result.Message = Message;
        UE_LOG_ERROR("[Build] Build Failed: %s", Message.c_str());
        return Result;
    }

    if (!PrepareBrandingResources(Settings, Message))
    {
        Result.Message = Message;
        UE_LOG_ERROR("[Build] Build Failed: %s", Message.c_str());
        return Result;
    }

    if (!RunMSBuild(Settings, Message))
    {
        FString IgnoredResetMessage;
        PrepareBrandingResources(FGameBuildSettings{}, IgnoredResetMessage);
        Result.Message = Message;
        UE_LOG_ERROR("[Build] Build Failed: %s", Message.c_str());
        return Result;
    }

    FString IgnoredResetMessage;
    PrepareBrandingResources(FGameBuildSettings{}, IgnoredResetMessage);

    if (!CopyPackageFiles(Settings, Message))
    {
        Result.Message = Message;
        UE_LOG_ERROR("[Build] Build Failed: %s", Message.c_str());
        return Result;
    }

    Result.bSucceeded = true;
    Result.Message = "Game package created";
    UE_LOG("[Build] Build Complete");
    return Result;
}

bool FGamePackager::PrepareBrandingResources(const FGameBuildSettings& Settings, FString& OutMessage)
{
    if (!ValidateOptionalBrandingFile(Settings.IconPath, { ".ico" }, "Game icon", OutMessage))
    {
        return false;
    }
    if (!ValidateOptionalBrandingFile(Settings.SplashImagePath, { ".png", ".jpg", ".jpeg", ".bmp" }, "Splash image", OutMessage))
    {
        return false;
    }

    const std::filesystem::path EngineRoot(FPaths::RootDir());
    const std::filesystem::path GeneratedDir = EngineRoot / L"Source" / L"Engine" / L"Runtime";
    const std::filesystem::path RcPath = GeneratedDir / L"GameBranding.rc";
    std::error_code Ec;
    std::filesystem::create_directories(GeneratedDir, Ec);
    if (Ec)
    {
        OutMessage = "Failed to create branding resource directory";
        EmitBuildLog(OutMessage);
        return false;
    }

    std::ofstream RcFile(RcPath, std::ios::trunc);
    if (!RcFile.is_open())
    {
        OutMessage = "Failed to write GameBranding.rc";
        EmitBuildLog(OutMessage);
        return false;
    }

    RcFile << "// Auto-generated by Packaging. Do not edit by hand.\n";
    if (!Settings.IconPath.empty())
    {
        const std::filesystem::path IconPath = ResolvePackageInputPath(Settings.IconPath);
        RcFile << "#define IDI_GAME_ICON 101\n";
        RcFile << "IDI_GAME_ICON ICON \"" << EscapeRcPath(IconPath) << "\"\n";
        EmitBuildLogWidePath("Generated exe icon resource = ", IconPath);
    }
    else
    {
        EmitBuildLog("Generated empty branding resource. No game icon selected.");
    }
    return true;
}

bool FGamePackager::RunMSBuild(const FGameBuildSettings& Settings, FString& OutMessage)
{
    const std::filesystem::path SolutionRoot = FindSolutionRoot();
    const std::filesystem::path SolutionPath = SolutionRoot / L"NipsEngine.sln";
    if (!std::filesystem::exists(SolutionPath))
    {
        OutMessage = "NipsEngine.sln not found";
        EmitBuildLog("NipsEngine.sln not found");
        EmitBuildLogWidePath("FPaths::RootDir = ", std::filesystem::path(FPaths::RootDir()));
        EmitBuildLogWidePath("Current path    = ", std::filesystem::current_path());
        return false;
    }

    const FString Configuration = GetConfigurationName(Settings.Configuration);
    EmitBuildLog("Starting MSBuild");
    EmitBuildLogWidePath("Solution = ", SolutionPath);
    UE_LOG("[Build] Configuration = %s", Configuration.c_str());

    std::wstring CommandLine =
        L"\"" + std::wstring(MSBuildPath) + L"\" \"" + SolutionPath.wstring()
        + L"\" /p:Configuration=" + FPaths::ToWide(Configuration)
        + L" /p:Platform=x64 /v:minimal";

    SECURITY_ATTRIBUTES SecurityAttributes = {};
    SecurityAttributes.nLength = sizeof(SecurityAttributes);
    SecurityAttributes.bInheritHandle = TRUE;

    HANDLE ReadPipe = nullptr;
    HANDLE WritePipe = nullptr;
    if (!CreatePipe(&ReadPipe, &WritePipe, &SecurityAttributes, 0))
    {
        OutMessage = "Failed to create MSBuild log pipe";
        EmitBuildLog(OutMessage);
        return false;
    }
    SetHandleInformation(ReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW StartupInfo = {};
    StartupInfo.cb = sizeof(StartupInfo);
    StartupInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    StartupInfo.wShowWindow = SW_HIDE;
    StartupInfo.hStdOutput = WritePipe;
    StartupInfo.hStdError = WritePipe;
    StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION ProcessInfo = {};
    std::wstring MutableCommandLine = CommandLine;
    if (!CreateProcessW(nullptr, MutableCommandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                        SolutionRoot.wstring().c_str(), &StartupInfo, &ProcessInfo))
    {
        CloseHandle(ReadPipe);
        CloseHandle(WritePipe);
        OutMessage = "Failed to start MSBuild";
        EmitBuildLog(OutMessage);
        return false;
    }

    CloseHandle(WritePipe);
    FString PendingOutput;
    while (WaitForSingleObject(ProcessInfo.hProcess, 50) == WAIT_TIMEOUT)
    {
        if (!ReadAvailablePipeOutput(ReadPipe, PendingOutput))
        {
            break;
        }
    }

    while (ReadAvailablePipeOutput(ReadPipe, PendingOutput))
    {
        DWORD AvailableBytes = 0;
        if (!PeekNamedPipe(ReadPipe, nullptr, 0, nullptr, &AvailableBytes, nullptr) || AvailableBytes == 0)
        {
            break;
        }
    }
    FlushProcessOutput(PendingOutput, true);
    CloseHandle(ReadPipe);

    DWORD ExitCode = 1;
    GetExitCodeProcess(ProcessInfo.hProcess, &ExitCode);
    CloseHandle(ProcessInfo.hThread);
    CloseHandle(ProcessInfo.hProcess);

    if (ExitCode != 0)
    {
        OutMessage = "MSBuild failed";
        UE_LOG_ERROR("[Build] MSBuild failed. ExitCode=%lu", ExitCode);
        return false;
    }

    EmitBuildLog("MSBuild succeeded");
    return true;
}

bool FGamePackager::CopyPackageFiles(const FGameBuildSettings& Settings, FString& OutMessage)
{
    const std::filesystem::path EngineRoot(FPaths::RootDir());
    const std::filesystem::path OutputRoot = ResolveAgainstProjectRoot(Settings.OutputDirectory);
    const FString Configuration = GetConfigurationName(Settings.Configuration);
    const std::filesystem::path BuiltExe = EngineRoot / L"Bin" / FPaths::ToWide(Configuration) / L"NipsGame.exe";
    const std::filesystem::path BuiltPdb = EngineRoot / L"Bin" / FPaths::ToWide(Configuration) / L"NipsGame.pdb";

    std::error_code Ec;
    if (!std::filesystem::exists(BuiltExe, Ec))
    {
        OutMessage = "Built NipsGame.exe not found";
        EmitBuildLog(OutMessage);
        EmitBuildLogWidePath("Expected exe = ", BuiltExe);
        return false;
    }

    if (Settings.bCleanOutput)
    {
        EmitBuildLogWidePath("Cleaning output = ", OutputRoot);
        std::filesystem::remove_all(OutputRoot, Ec);
        if (Ec)
        {
            OutMessage = "Failed to clean output directory";
            EmitBuildLog(OutMessage);
            return false;
        }
    }

    EmitBuildLogWidePath("Packaging to = ", OutputRoot);
    std::filesystem::create_directories(OutputRoot, Ec);
    if (Ec)
    {
        OutMessage = "Failed to create output directory";
        EmitBuildLog(OutMessage);
        return false;
    }

    std::filesystem::copy_file(BuiltExe, OutputRoot / L"NipsGame.exe",
                               std::filesystem::copy_options::overwrite_existing, Ec);
    if (Ec)
    {
        OutMessage = "Failed to copy NipsGame.exe";
        EmitBuildLog(OutMessage);
        return false;
    }
    EmitBuildLog("Copied NipsGame.exe");

    if (std::filesystem::exists(BuiltPdb, Ec))
    {
        std::filesystem::copy_file(BuiltPdb, OutputRoot / L"NipsGame.pdb",
                                   std::filesystem::copy_options::overwrite_existing, Ec);
        if (Ec)
        {
            OutMessage = "Failed to copy NipsGame.pdb";
            EmitBuildLog(OutMessage);
            return false;
        }
        EmitBuildLog("Copied NipsGame.pdb");
    }

    const std::filesystem::path LuaJitRuntimeDll = EngineRoot / L"ThirdParty" / L"luajit" / L"src" / L"lua51.dll";
    if (!std::filesystem::exists(LuaJitRuntimeDll, Ec))
    {
        OutMessage = "LuaJIT runtime dll not found: ThirdParty/luajit/src/lua51.dll";
        EmitBuildLog(OutMessage);
        return false;
    }

    std::filesystem::copy_file(
        LuaJitRuntimeDll,
        OutputRoot / L"lua51.dll",
        std::filesystem::copy_options::overwrite_existing,
        Ec);
    if (Ec)
    {
        OutMessage = "Failed to copy LuaJIT runtime dll";
        EmitBuildLog(OutMessage);
        return false;
    }
    EmitBuildLog("Copied LuaJIT runtime dll");

    const TArray<FString> IncludedScenes = BuildIncludedSceneList(Settings);
    if (!CookAndCopyPackageAssets(Settings, OutputRoot, IncludedScenes, OutMessage)) return false;
    if (!CopyDirectoryIfExists(EngineRoot / L"Shaders", OutputRoot / L"Shaders", OutMessage)) return false;
    EmitBuildLog("Copied Shaders directory");
    if (!CopyDirectoryIfExists(EngineRoot / L"DerivedData" / L"ShaderCache", OutputRoot / L"DerivedData" / L"ShaderCache", OutMessage)) return false;
    EmitBuildLog("Copied ShaderCache directory");
    if (!CopyDirectoryIfExists(EngineRoot / L"Asset" / L"UI", OutputRoot / L"Asset" / L"UI", OutMessage)) return false;
    EmitBuildLog("Copied Asset/UI directory");
    if (!CopyDirectoryIfExists(EngineRoot / L"Asset" / L"UIImage", OutputRoot / L"Asset" / L"UIImage", OutMessage)) return false;
    EmitBuildLog("Copied Asset/UIImage directory");
    if (!CopyDirectoryIfExists(EngineRoot / L"Asset" / L"UIFont", OutputRoot / L"Asset" / L"UIFont", OutMessage)) return false;
    EmitBuildLog("Copied Asset/UIFont directory");
    if (!CopyDirectoryIfExists(EngineRoot / L"Asset" / L"Script", OutputRoot / L"Asset" / L"Script", OutMessage)) return false;
    EmitBuildLog("Copied Asset/Script directory");
    if (!CopyDirectoryIfExists(EngineRoot / L"Asset" / L"Prefab", OutputRoot / L"Asset" / L"Prefab", OutMessage)) return false;
    EmitBuildLog("Copied Asset/Prefab directory");
    if (!CopyDirectoryIfExists(EngineRoot / L"LuaScript", OutputRoot / L"LuaScript", OutMessage)) return false;
    EmitBuildLog("Copied LuaScript directory");
    if (!CopyDirectoryIfExists(EngineRoot / L"Asset" / L"Audio", OutputRoot / L"Asset" / L"Audio", OutMessage)) return false;
    EmitBuildLog("Copied Asset/Audio directory");

    FString PackagedIconPath;
    FString PackagedSplashPath;
    if (!Settings.IconPath.empty() || !Settings.SplashImagePath.empty())
    {
        const std::filesystem::path BrandingDir = OutputRoot / L"Branding";
        std::filesystem::create_directories(BrandingDir, Ec);
        if (Ec)
        {
            OutMessage = "Failed to create Branding directory";
            EmitBuildLog(OutMessage);
            return false;
        }

        if (!Settings.IconPath.empty())
        {
            const std::filesystem::path SourceIcon = ResolvePackageInputPath(Settings.IconPath);
            const std::filesystem::path DestIcon = BrandingDir / SourceIcon.filename();
            std::filesystem::copy_file(SourceIcon, DestIcon, std::filesystem::copy_options::overwrite_existing, Ec);
            if (Ec)
            {
                OutMessage = "Failed to copy game icon";
                EmitBuildLog(OutMessage);
                return false;
            }
            PackagedIconPath = NormalizePackagePath(std::filesystem::path(L"Branding") / SourceIcon.filename());
            EmitBuildLog("Copied game icon");
        }

        if (!Settings.SplashImagePath.empty())
        {
            const std::filesystem::path SourceSplash = ResolvePackageInputPath(Settings.SplashImagePath);
            const std::filesystem::path DestSplash = BrandingDir / SourceSplash.filename();
            std::filesystem::copy_file(SourceSplash, DestSplash, std::filesystem::copy_options::overwrite_existing, Ec);
            if (Ec)
            {
                OutMessage = "Failed to copy splash image";
                EmitBuildLog(OutMessage);
                return false;
            }
            PackagedSplashPath = NormalizePackagePath(std::filesystem::path(L"Branding") / SourceSplash.filename());
            EmitBuildLog("Copied splash image");
        }
    }

    std::filesystem::create_directories(OutputRoot / L"Settings", Ec);
    std::filesystem::create_directories(OutputRoot / L"Saves", Ec);

    std::ofstream GameIni(OutputRoot / L"Settings" / L"Game.ini", std::ios::trunc);
    if (!GameIni.is_open())
    {
        OutMessage = "Failed to write Game.ini";
        EmitBuildLog(OutMessage);
        return false;
    }

    GameIni << "[Game]\n";
    GameIni << "GameName=" << Settings.GameName << "\n";
    GameIni << "StartupScene=" << NormalizeSceneForPackage(Settings.StartupScene) << "\n";
    GameIni << "PlayerControllerClass=" << Settings.PlayerControllerClass << "\n";
    GameIni << "\n[Scenes]\n";
    GameIni << "Count=" << IncludedScenes.size() << "\n";
    for (size_t SceneIndex = 0; SceneIndex < IncludedScenes.size(); ++SceneIndex)
    {
        GameIni << "Scene" << SceneIndex << "=" << IncludedScenes[SceneIndex] << "\n";
    }
    GameIni << "\n[Branding]\n";
    GameIni << "Icon=" << PackagedIconPath << "\n";
    GameIni << "Splash=" << PackagedSplashPath << "\n";
    GameIni << "SplashMinSeconds=" << std::max(3.0f, Settings.SplashMinSeconds) << "\n";
    GameIni.close();
    EmitBuildLog("Wrote Settings/Game.ini");

    if (Settings.bRunAfterBuild)
    {
        const std::filesystem::path PackageExe = OutputRoot / L"NipsGame.exe";
        STARTUPINFOW StartupInfo = {};
        StartupInfo.cb = sizeof(StartupInfo);
        PROCESS_INFORMATION ProcessInfo = {};
        std::wstring CommandLine = L"\"" + PackageExe.wstring() + L"\"";
        if (!CreateProcessW(nullptr, CommandLine.data(), nullptr, nullptr, FALSE, 0, nullptr,
                            OutputRoot.wstring().c_str(), &StartupInfo, &ProcessInfo))
        {
            OutMessage = "Package created, but failed to run NipsGame.exe";
            return false;
        }
        CloseHandle(ProcessInfo.hThread);
        CloseHandle(ProcessInfo.hProcess);
    }

    return true;
}
