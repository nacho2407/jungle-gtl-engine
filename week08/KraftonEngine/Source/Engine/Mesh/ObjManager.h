// 메시 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Object/ObjectIterator.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include <map>
#include <string>
#include <memory>

struct FStaticMesh;
struct FStaticMaterial;
struct FImportOptions;
class UStaticMesh;

// FMeshAssetListItem는 메시 데이터와 렌더 제출 정보를 다룹니다.
struct FMeshAssetListItem
{
    FString DisplayName;
    FString FullPath;
};

// FObjManager는 관련 객체의 생성, 조회, 수명 관리를 담당합니다.
class FObjManager
{
    // path → UStaticMesh* 캐시 (소유권은 UObjectManager)
    static TMap<FString, UStaticMesh*> StaticMeshCache;
    static TArray<FMeshAssetListItem> AvailableMeshFiles;
    static TArray<FMeshAssetListItem> AvailableObjFiles;

public:
    // 원본 경로(.obj 등)를 대응되는 바이너리 캐시 경로로 변환한다.
    // 이미 .bin 이면 그대로 반환한다.
    static FString GetBinaryFilePath(const FString& OriginalPath);

    // 이전 패치/로컬 코드와의 호환용 별칭
    static FString GetBinaryFilePathString(const FString& OriginalPath) { return GetBinaryFilePath(OriginalPath); }

    static UStaticMesh* LoadObjStaticMesh(const FString& PathFileName, ID3D11Device* InDevice);
    static UStaticMesh* LoadObjStaticMesh(const FString& PathFileName, const FImportOptions& Options, ID3D11Device* InDevice);
    static void ScanMeshAssets();
    static const TArray<FMeshAssetListItem>& GetAvailableMeshFiles();
    static void ScanObjSourceFiles();
    static const TArray<FMeshAssetListItem>& GetAvailableObjFiles();

    // 캐시된 StaticMesh GPU 리소스 해제 (Shutdown 시 Device 해제 전 호출)
    static void ReleaseAllGPU();

private:
    static bool TryImportStaticMesh(const FString& ObjPath, const FImportOptions* Options, UStaticMesh* StaticMesh, const FString& BinPath);
};
