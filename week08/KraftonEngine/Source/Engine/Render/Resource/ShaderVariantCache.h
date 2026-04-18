#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resource/Shader.h"

#include <cstdlib>
#include <memory>
#include <string>

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
        for (const FShaderMacroDefine &Def : Defines)
        {
            Key += "|" + Def.Name + "=" + Def.Value;
        }
        return Key;
    }
};

class FShaderVariantCache
{
  public:
    void Initialize(ID3D11Device *InDevice)
    {
        Device = InDevice;
    }

    void Release()
    {
        for (auto &Pair : Cache)
        {
            if (Pair.second)
            {
                Pair.second->Release();
            }
        }
        Cache.clear();
        Device = nullptr;
    }

    FShader *GetOrCreate(const FShaderVariantDesc &Desc)
    {
        if (!Device)
        {
            return nullptr;
        }

        const FString Key = Desc.BuildKey();
        auto It = Cache.find(Key);
        if (It != Cache.end())
        {
            return It->second.get();
        }

        auto NewShader = std::make_unique<FShader>();
        TArray<D3D_SHADER_MACRO> D3DDefines;
        BuildD3DDefines(Desc.Defines, D3DDefines);

        const std::wstring WidePath(Desc.FilePath.begin(), Desc.FilePath.end());
        NewShader->Create(Device, WidePath.c_str(), Desc.VSEntry.c_str(), Desc.PSEntry.c_str(),
                          D3DDefines.empty() ? nullptr : D3DDefines.data());

        for (D3D_SHADER_MACRO &Macro : D3DDefines)
        {
            if (Macro.Name)
            {
                free(const_cast<char *>(Macro.Name));
            }
            if (Macro.Definition)
            {
                free(const_cast<char *>(Macro.Definition));
            }
        }

        FShader *RawShader = NewShader.get();
        Cache.emplace(Key, std::move(NewShader));
        return RawShader;
    }

  private:
    void BuildD3DDefines(const TArray<FShaderMacroDefine> &InDefines, TArray<D3D_SHADER_MACRO> &OutDefines) const
    {
        OutDefines.clear();
        OutDefines.reserve(InDefines.size() + 1);

        for (const FShaderMacroDefine &Def : InDefines)
        {
            D3D_SHADER_MACRO Macro = {};
            Macro.Name = _strdup(Def.Name.c_str());
            Macro.Definition = _strdup(Def.Value.c_str());
            OutDefines.push_back(Macro);
        }

        OutDefines.push_back({nullptr, nullptr});
    }

  private:
    ID3D11Device *Device = nullptr;
    TMap<FString, std::unique_ptr<FShader>> Cache;
};
