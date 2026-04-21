#pragma once

#include <d3dcommon.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/*
    HLSL #include를 실제 파일로 해석해 주는 D3D11 include 로더입니다.
    include된 파일 목록도 함께 수집해서 셰이더 변경 감시에 재사용합니다.
*/
class FShaderIncludeLoader final : public ID3DInclude
{
public:
    explicit FShaderIncludeLoader(const std::filesystem::path& InRootShaderFile,
                                  std::unordered_set<std::wstring>* InOutDependencies = nullptr)
        : RootShaderFile(MakeNormalizedPath(InRootShaderFile)), RootDirectory(RootShaderFile.parent_path()), Dependencies(InOutDependencies)
    {
        RegisterDependency(RootShaderFile);
    }

    HRESULT Open(D3D_INCLUDE_TYPE IncludeType,
                 LPCSTR pFileName,
                 LPCVOID pParentData,
                 LPCVOID* ppData,
                 UINT* pBytes) override
    {
        (void)IncludeType;

        if (pFileName == nullptr || ppData == nullptr || pBytes == nullptr)
        {
            return E_INVALIDARG;
        }

        std::filesystem::path BaseDirectory = RootDirectory;
        if (pParentData != nullptr)
        {
            const auto Found = BufferToDirectory.find(pParentData);
            if (Found != BufferToDirectory.end())
            {
                BaseDirectory = Found->second;
            }
        }

        const std::filesystem::path ResolvedPath = ResolveIncludePath(BaseDirectory, std::filesystem::path(pFileName));

        std::ifstream File(ResolvedPath, std::ios::binary | std::ios::ate);
        if (!File.is_open())
        {
            return E_FAIL;
        }

        const std::streamsize Size = File.tellg();
        if (Size < 0)
        {
            return E_FAIL;
        }

        File.seekg(0, std::ios::beg);

        std::unique_ptr<char[]> Buffer = std::make_unique<char[]>(static_cast<size_t>(Size));
        if (Size > 0)
        {
            File.read(Buffer.get(), Size);
            if (!File)
            {
                return E_FAIL;
            }
        }

        const void* BufferKey = Buffer.get();
        OwnedBuffers.emplace(BufferKey, std::move(Buffer));
        BufferToDirectory.emplace(BufferKey, ResolvedPath.parent_path());
        RegisterDependency(ResolvedPath);

        *ppData = BufferKey;
        *pBytes = static_cast<UINT>(Size);
        return S_OK;
    }

    HRESULT Close(LPCVOID pData) override
    {
        BufferToDirectory.erase(pData);
        OwnedBuffers.erase(pData);
        return S_OK;
    }

private:
    std::filesystem::path ResolveIncludePath(const std::filesystem::path& BaseDirectory, const std::filesystem::path& IncludePath) const
    {
        if (IncludePath.is_absolute())
        {
            return MakeNormalizedPath(IncludePath);
        }

        std::vector<std::filesystem::path> SearchRoots;
        SearchRoots.push_back(BaseDirectory);
        SearchRoots.push_back(RootDirectory);

        std::filesystem::path Cursor = RootDirectory;
        while (!Cursor.empty())
        {
            SearchRoots.push_back(Cursor);
            SearchRoots.push_back(Cursor / "Shaders");
            if (Cursor.filename() == "Shaders")
            {
                SearchRoots.push_back(Cursor.parent_path());
            }
            if (Cursor == Cursor.root_path())
            {
                break;
            }
            Cursor = Cursor.parent_path();
        }

        for (const std::filesystem::path& Root : SearchRoots)
        {
            std::error_code EC;
            const std::filesystem::path Candidate = MakeNormalizedPath(Root / IncludePath);
            if (std::filesystem::exists(Candidate, EC) && !EC)
            {
                return Candidate;
            }
        }

        return MakeNormalizedPath(BaseDirectory / IncludePath);
    }

    static std::filesystem::path MakeNormalizedPath(const std::filesystem::path& InPath)
    {
        std::error_code EC;
        std::filesystem::path Path = std::filesystem::absolute(InPath, EC);
        if (EC)
        {
            EC.clear();
            Path = InPath;
        }

        std::filesystem::path Canonical = std::filesystem::weakly_canonical(Path, EC);
        if (EC)
        {
            return Path.lexically_normal();
        }
        return Canonical;
    }

    void RegisterDependency(const std::filesystem::path& InPath)
    {
        if (Dependencies == nullptr)
        {
            return;
        }

        Dependencies->insert(MakeNormalizedPath(InPath).wstring());
    }

private:
    std::filesystem::path RootShaderFile;
    std::filesystem::path RootDirectory;
    std::unordered_set<std::wstring>* Dependencies = nullptr;
    std::unordered_map<const void*, std::unique_ptr<char[]>> OwnedBuffers;
    std::unordered_map<const void*, std::filesystem::path> BufferToDirectory;
};
