// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

/*
    셰이더 컴파일러에 전달할 전처리 define입니다.
    각 항목은 D3D_SHADER_MACRO의 Name/Definition으로 변환됩니다.
*/
struct FShaderCompileDefine
{
    FString Name;
    FString Value;
};

/*
    단일 셰이더 stage를 컴파일하기 위한 파일, 엔트리, define 정보입니다.
    shader target 문자열은 desc에 넣지 않고 program 구현 내부에서 고정합니다.
*/
struct FShaderStageDesc
{
    FString                      FilePath;
    FString                      EntryPoint;
    TArray<FShaderCompileDefine> Defines;
};

/*
    그래픽 프로그램을 구성하는 셰이더 stage 설명입니다.
    VS는 필수이고 PS는 depth-only 같은 VS-only 패스를 위해 선택 사항입니다.
*/
struct FGraphicsProgramDesc
{
    FString DebugName;

    FShaderStageDesc            VS;
    TOptional<FShaderStageDesc> PS;
};

/*
    컴퓨트 프로그램을 구성하는 셰이더 stage 설명입니다.
    CS는 필수이며 graphics stage와 bind 규칙을 공유하지 않습니다.
*/
struct FComputeProgramDesc
{
    FString DebugName;

    FShaderStageDesc CS;
};
