#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

/**
 * @brief MTL 파일의 머테리얼 데이터를 표현하는 구조체.
 * Obj .mtl 포맷 기준으로 정의했습니다.
 */

struct FMaterial
{
    FString Name;

    FVector AmbientColor   = { 0.2f, 0.2f, 0.2f }; // Ka
    FVector DiffuseColor   = { 0.8f, 0.8f, 0.8f }; // Kd
    FVector SpecularColor  = { 0.0f, 0.0f, 0.0f }; // Ks
    FVector EmissiveColor  = { 0.0f, 0.0f, 0.0f }; // Ke


    float Shininess  = 0.0f; 
    float Opacity    = 1.0f; 
    int   IllumModel = 2;    

	// 경로는 파싱 시점에 절대 경로로 정규화됨
    FString DiffuseTexPath;   // map_Kd
    FString AmbientTexPath;   // map_Ka
    FString SpecularTexPath;  // map_Ks
    FString BumpTexPath;      // map_bump
};

/**
 * @brief Obj전용 .mtl 파일 파서
 * ResourceManager에서 TMap<FString, FMaterial>을 참조로 받아와 채워 놓을 예정
 */
class FObjMtlLoader
{
public:
    /**
     * @brief MTL 파일을 파싱하여 머테리얼 맵을 채웁니다.
     * @param FilePath
     * @param OutMaterials 
     * @return 파일 열기 성공 여부 
     */
    static bool Load(const FString& FilePath, TMap<FString, FMaterial>& OutMaterials);
};
