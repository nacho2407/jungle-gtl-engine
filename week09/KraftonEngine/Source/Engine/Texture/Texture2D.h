// 텍스처 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Object/Object.h"
#include "Core/CoreTypes.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct ID3D11Device;
struct ID3D11ShaderResourceView;

// FTextureCacheEntry는 텍스처 리소스의 로드와 GPU 사용을 다룹니다.
struct FTextureCacheEntry
{
    class UTexture2D* Texture = nullptr;
    FString FullPath;
    std::filesystem::file_time_type LastWriteTime{};
    bool bExists = false;
};

// UTexture2D는 텍스처 리소스의 로드와 GPU 사용을 다룹니다.
class UTexture2D : public UObject
{
public:
    DECLARE_CLASS(UTexture2D, UObject)

    UTexture2D() = default;
    ~UTexture2D() override;

    static UTexture2D* LoadFromFile(const FString& FilePath, ID3D11Device* Device);
    static void ReleaseAllGPU();

    ID3D11ShaderResourceView* GetSRV() const { return SRV; }
    const FString& GetSourcePath() const { return SourceFilePath; }
    uint32 GetWidth() const { return Width; }
    uint32 GetHeight() const { return Height; }
    bool IsLoaded() const { return SRV != nullptr; }

private:
    bool LoadInternal(const FString& FilePath, ID3D11Device* Device);
    static std::filesystem::path ResolveFullPath(const FString& FilePath);
    static FTextureCacheEntry BuildCacheEntry(const std::filesystem::path& FilePath);
    static bool HasCacheEntryChanged(const FTextureCacheEntry& Entry);

    FString SourceFilePath;
    ID3D11ShaderResourceView* SRV = nullptr;
    uint32 Width = 0;
    uint32 Height = 0;
    uint64 TrackedTextureMemory = 0;

    static std::map<FString, FTextureCacheEntry> TextureCache;
    static std::vector<UTexture2D*> RetiredTextures;
};
