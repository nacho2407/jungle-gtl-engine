// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Shaders/GraphicsProgram.h"
#include "Render/Resources/Shaders/ShaderDependencyUtils.h"

#include <memory>

/*
    뷰 모드 variant를 만들 때 추가할 셰이더 매크로입니다.
*/
struct FShaderMacroDefine
{
    FString Name;
    FString Value;
};

/*
    하나의 뷰 모드 셰이더 permutation을 식별하는 파일, 엔트리, 매크로 묶음입니다.
*/
struct FShaderVariantDesc
{
    FString                    FilePath;
    FString                    VSEntry;
    FString                    PSEntry;
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

/*
    컴파일된 뷰 모드 variant 프로그램과 원본 파일 의존성을 함께 저장합니다.
*/
struct FShaderVariantCacheEntry
{
    std::unique_ptr<FGraphicsProgram> Shader;
    FShaderVariantFileDependency      SourceFile;
    FShaderVariantDesc                Desc;
};

// FShaderVariantCache는 셰이더 컴파일 결과와 GPU 바인딩을 관리합니다.
class FShaderVariantCache
{
public:
    // ========== 생명 주기 ==========

    /*
        variant 컴파일에 사용할 D3D 디바이스를 등록합니다.
    */
    void Initialize(ID3D11Device* InDevice)
    {
        Device = InDevice;
    }

    /*
        캐시된 variant 프로그램을 모두 해제합니다.
    */
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

    /*
        캐시된 variant의 원본 파일 변경을 검사하고 필요한 항목을 다시 컴파일합니다.
    */
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

    // ========== 조회 ==========

    /*
        desc에 해당하는 variant 프로그램을 찾거나 새로 컴파일합니다.
    */
    FGraphicsProgram* GetOrCreate(const FShaderVariantDesc& Desc)
    {
        if (!Device)
        {
            return nullptr;
        }

        const FString Key = Desc.BuildKey();
        auto          It  = Cache.find(Key);
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
        Entry.Shader = std::make_unique<FGraphicsProgram>();
        Entry.Desc   = Desc;
        if (!RecompileEntry(Desc, Entry))
        {
            return nullptr;
        }

        FGraphicsProgram* RawShader = Entry.Shader.get();
        Cache.emplace(Key, std::move(Entry));
        return RawShader;
    }

private:
    // ========== 컴파일 ==========

    /*
        variant desc를 그래픽 프로그램 desc로 변환해 컴파일합니다.
    */
    bool RecompileEntry(const FShaderVariantDesc& Desc, FShaderVariantCacheEntry& Entry)
    {
        if (!Device)
        {
            return false;
        }

        if (!Entry.Shader)
        {
            Entry.Shader = std::make_unique<FGraphicsProgram>();
        }

        FGraphicsProgramDesc ProgramDesc;
        ProgramDesc.DebugName     = Desc.BuildKey();
        ProgramDesc.VS.FilePath   = Desc.FilePath;
        ProgramDesc.VS.EntryPoint = Desc.VSEntry;
        ProgramDesc.PS            = FShaderStageDesc{ Desc.FilePath, Desc.PSEntry, {} };

        for (const FShaderMacroDefine& Define : Desc.Defines)
        {
            FShaderCompileDefine CompileDefine{ Define.Name, Define.Value };
            ProgramDesc.VS.Defines.push_back(CompileDefine);
            ProgramDesc.PS->Defines.push_back(CompileDefine);
        }

        if (Entry.Shader->Create(Device, ProgramDesc))
        {
            Entry.SourceFile = ShaderDependencyUtils::BuildFileDependency(Desc.FilePath);
            return true;
        }
        return false;
    }

private:
    ID3D11Device*                           Device = nullptr;
    TMap<FString, FShaderVariantCacheEntry> Cache;
};
