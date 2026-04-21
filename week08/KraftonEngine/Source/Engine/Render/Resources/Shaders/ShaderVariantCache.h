#pragma once

#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Shaders/GraphicsShaderProgram.h"
#include "Render/Resources/Shaders/ShaderDependencyUtils.h"

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <system_error>

struct FShaderMacroDefine
{
    FString Name;
    FString Value;
};

struct FShaderVariantDesc
{
    FString FilePath;
    FString VSEntry;
    FString PSEntry;
    TArray<FShaderMacroDefine> Defines;

    FString BuildKey() const
    {
        FString Key = FilePath + "|VS=" + VSEntry + "|PS=" + PSEntry;
        for (const FShaderMacroDefine& Def : Defines)
        {
            Key += "|" + Def.Name + "=" + Def.Value;
        }
        return Key;
    }
};

using FShaderVariantFileDependency = ShaderDependencyUtils::FShaderFileDependency;

struct FShaderVariantCacheEntry
{
    std::unique_ptr<FShader> Shader;
    FShaderVariantFileDependency SourceFile;
    FShaderVariantDesc Desc;
};

class FShaderVariantCache
{
public:
    void Initialize(ID3D11Device* InDevice)
    {
        Device = InDevice;
    }

    void Release()
    {
        for (auto& Pair : Cache)
        {
            if (Pair.second.Shader)
            {
                Pair.second.Shader->Release();
            }
        }
        Cache.clear();

        Device = nullptr;
    }

    void TickHotReload()
    {
        if (!Device)
        {
            return;
        }

        for (auto& Pair : Cache)
        {
            auto& Entry = Pair.second;
            if (!Entry.Shader)
            {
                continue;
            }

            if (Entry.SourceFile.bExists && !ShaderDependencyUtils::HasDependencyChanged(Entry.SourceFile))
            {
                continue;
            }

            const bool bReloaded = RecompileEntry(Entry.Desc, Entry);
            (void)bReloaded;
        }
    }

    FShader* GetOrCreate(const FShaderVariantDesc& Desc)
    {
        if (!Device)
        {
            return nullptr;
        }

        const FString Key = Desc.BuildKey();
        auto It = Cache.find(Key);
        if (It != Cache.end())
        {
            auto& Entry = It->second;
            if (Entry.SourceFile.bExists && !ShaderDependencyUtils::HasDependencyChanged(Entry.SourceFile))
            {
                return Entry.Shader.get();
            }

            Entry.Desc = Desc;
            RecompileEntry(Desc, Entry);
            return Entry.Shader.get();
        }

        FShaderVariantCacheEntry Entry;
        Entry.Shader = std::make_unique<FShader>();
        Entry.Desc = Desc;
        if (!RecompileEntry(Desc, Entry))
        {
            return nullptr;
        }

        FShader* RawShader = Entry.Shader.get();
        Cache.emplace(Key, std::move(Entry));
        return RawShader;
    }

private:
    bool RecompileEntry(const FShaderVariantDesc& Desc, FShaderVariantCacheEntry& Entry)
    {
        if (!Device)
        {
            return false;
        }

        if (!Entry.Shader)
        {
            Entry.Shader = std::make_unique<FShader>();
        }

        TArray<D3D_SHADER_MACRO> D3DDefines;
        BuildD3DDefines(Desc.Defines, D3DDefines);

        std::filesystem::path AbsolutePath = FPaths::ToPath(FPaths::ToWide(Desc.FilePath));
        if (!AbsolutePath.is_absolute())
        {
            AbsolutePath = FPaths::ToPath(FPaths::RootDir()) / AbsolutePath;
        }

        std::error_code EC;
        AbsolutePath = std::filesystem::weakly_canonical(AbsolutePath, EC);
        if (EC)
        {
            AbsolutePath = AbsolutePath.lexically_normal();
        }

        const bool bCompiled = Entry.Shader->Create(Device, AbsolutePath.wstring().c_str(), Desc.VSEntry.c_str(), Desc.PSEntry.c_str(), D3DDefines.empty() ? nullptr : D3DDefines.data());

        ReleaseD3DDefines(D3DDefines);

        if (bCompiled)
        {
            Entry.SourceFile = ShaderDependencyUtils::BuildFileDependency(Desc.FilePath);
        }
        return bCompiled;
    }

    void ReleaseD3DDefines(TArray<D3D_SHADER_MACRO>& InOutDefines) const
    {
        for (D3D_SHADER_MACRO& Macro : InOutDefines)
        {
            if (Macro.Name)
            {
                free(const_cast<char*>(Macro.Name));
            }
            if (Macro.Definition)
            {
                free(const_cast<char*>(Macro.Definition));
            }
        }
        InOutDefines.clear();
    }

    void BuildD3DDefines(const TArray<FShaderMacroDefine>& InDefines, TArray<D3D_SHADER_MACRO>& OutDefines) const
    {
        OutDefines.clear();
        OutDefines.reserve(InDefines.size() + 1);

        for (const FShaderMacroDefine& Def : InDefines)
        {
            D3D_SHADER_MACRO Macro = {};
            Macro.Name = _strdup(Def.Name.c_str());
            Macro.Definition = _strdup(Def.Value.c_str());
            OutDefines.push_back(Macro);
        }

        OutDefines.push_back({ nullptr, nullptr });
    }

private:
    ID3D11Device* Device = nullptr;
    TMap<FString, FShaderVariantCacheEntry> Cache;
};
