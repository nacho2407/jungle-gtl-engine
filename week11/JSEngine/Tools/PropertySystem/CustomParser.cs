using System;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Collections.Generic; // List를 사용하기 위해 추가

class CustomParser
{
    static string RemoveComments(string code)
    {
        return Regex.Replace(
            code,
            @"//.*?$|/\*.*?\*/",
            "",
            RegexOptions.Multiline | RegexOptions.Singleline);
    }

    static void Main(string[] args)
    {
        Console.OutputEncoding = Encoding.UTF8;

        // 1. 프로젝트 루트 경로 자동 탐색
        string currentDir = args.Length > 0 ? args[0] : Directory.GetCurrentDirectory();
        currentDir = Path.GetFullPath(currentDir);

        Console.WriteLine($"\n[UHT] 파서 실행 위치: {currentDir}");

        string projectRootDir = currentDir;
        while (!Directory.Exists(Path.Combine(projectRootDir, "Source")))
        {
            DirectoryInfo parent = Directory.GetParent(projectRootDir);
            if (parent == null)
            {
                Console.WriteLine($"[오류] 최상단 드라이브까지 뒤졌지만 'Source' 폴더를 찾지 못했습니다.");
                return;
            }
            projectRootDir = parent.FullName;
        }

        string sourceDir = Path.Combine(projectRootDir, "Source");
        string outputDir = Path.Combine(projectRootDir, "Intermediate", "Generated");

        if (!Directory.Exists(outputDir))
        {
            Directory.CreateDirectory(outputDir);
        }

        Console.WriteLine($"[UHT] ✅ 공통 부모 폴더 발견: {projectRootDir}");
        Console.WriteLine($"[UHT] 📂 파싱 대상: {sourceDir}");
        Console.WriteLine($"[UHT] 💾 출력 폴더: {outputDir}");

        var headerFiles = Directory.GetFiles(sourceDir, "*.h", SearchOption.AllDirectories);

        int parsedCount = 0; // 파싱된 클래스 개수 카운터

        // ★ 마스터 파일에 포함할 .gen.cpp 이름들을 담아둘 리스트 (루프 바깥에 선언!)
        List<string> generatedCppFiles = new List<string>();

        // =========================================================================
        // [루프 시작] 모든 헤더 파일을 하나씩 검사합니다.
        // =========================================================================
        foreach (var file in headerFiles)
        {
            if (file.EndsWith(".generated.h")) continue;

            string cppCode = File.ReadAllText(file);
            string parseCode = RemoveComments(cppCode);

            // UCLASS 매크로 찾기
            Match classMatch = Regex.Match(parseCode, @"UCLASS\s*\([^)]*\)\s*class\s+(?:[A-Z0-9_]+_API\s+)?(\w+)");
            if (!classMatch.Success) continue;

            parsedCount++; // 클래스를 찾았으므로 카운트 증가!

            string className = classMatch.Groups[1].Value; // 예: UStaticMeshComponent
            string fileNameOnly = Path.GetFileNameWithoutExtension(file); // 예: StaticMeshComponent

            Console.WriteLine($"\n🎯 [파싱 성공] 타겟 클래스 발견: {className} (파일: {fileNameOnly}.h)");

            string generatedHeaderPath = Path.Combine(outputDir, $"{fileNameOnly}.generated.h");
            string generatedCppPath = Path.Combine(outputDir, $"{fileNameOnly}.gen.cpp");

            string includePath = file.StartsWith(sourceDir + Path.DirectorySeparatorChar)
                ? file.Substring(sourceDir.Length + 1).Replace('\\', '/')
                : Path.GetFileName(file);

            // [A] Header 내용 조립
            string headerContent = $@"// [UHT 자동 생성 헤더 파일 - 절대 직접 수정하지 마세요!]
#pragma once

extern void Register_{className}();

#undef GENERATED_BODY
#define GENERATED_BODY() \
public: \
    friend void Register_{className}(); \
    inline static FClassInfo StaticClassInfo; \
    virtual FClassInfo* GetStaticClass() override {{ return &StaticClassInfo; }}
";

            // [B] CPP 내용 조립
            string cppContent = $@"// [UHT 자동 생성 소스 파일 - 절대 직접 수정하지 마세요!]
#include ""{includePath}""
#include ""Core/ReflectionDatabase.h"" 

void Register_{className}() {{
    FClassInfo& info = {className}::StaticClassInfo;
    info.ClassName = ""{className}"";
";

            // 변수(UPROPERTY) 추출
            string pattern = @"UPROPERTY\s*\((.*?)\)\s+([A-Za-z_]\w*(?:::\w+)*(?:\s*<[^;{}()]+>)?\s*[*&]?)\s+(\w+)\s*(?:=.*?)?;";
            MatchCollection properties = Regex.Matches(parseCode, pattern, RegexOptions.Singleline);

            foreach (Match prop in properties)
            {
                string options = prop.Groups[1].Value;
                string type = prop.Groups[2].Value.Trim();
                string name = prop.Groups[3].Value;

                Console.WriteLine($"   ㄴ 변수 수집: {type} {name} (옵션: {options})");

                bool bIsEditAnywhere = options.Contains("EditAnywhere");
                string boolStr = bIsEditAnywhere ? "true" : "false";

                cppContent += $"    info.Properties.push_back({{ \"{name}\", \"{type}\", offsetof({className}, {name}), {boolStr} }});\n";

                if (type.Contains("*"))
                {
                    cppContent += $"    info.GcPointerOffsets.push_back(offsetof({className}, {name})); // GC 추적 대상\n";
                }
            }

            cppContent += $@"
    ReflectionDatabase::AddClass(""{className}"", &info);
}}

struct FAutoRegister_{className} {{
    FAutoRegister_{className}() {{
        Register_{className}();
    }}
}};
static FAutoRegister_{className} AutoRegister_{className}_Instance;
";

            // [C] 개별 파일 저장
            File.WriteAllText(generatedHeaderPath, headerContent);
            File.WriteAllText(generatedCppPath, cppContent);

            // ★ 핵심 수정: className이 아니라 실제 만들어진 파일 이름(fileNameOnly)을 리스트에 저장합니다.
            generatedCppFiles.Add($"{fileNameOnly}.gen.cpp");

            Console.WriteLine($"   ✅ 생성 완료: {fileNameOnly}.generated.h");
            Console.WriteLine($"   ✅ 생성 완료: {fileNameOnly}.gen.cpp");
        }
        // =========================================================================
        // [루프 종료]
        // =========================================================================

        // [D] 마스터 파일 생성 (루프가 완전히 끝난 밖에서 딱 1번만 실행!)
        string masterCppPath = Path.Combine(outputDir, "JSEngine.Reflection.gen.cpp");
        StringBuilder masterContent = new StringBuilder();

        masterContent.AppendLine("// [UHT 마스터 소스 파일 - 절대 직접 수정하지 마세요!]");
        masterContent.AppendLine("// 이 파일 하나만 비주얼 스튜디오에 포함시키면 모든 리플렉션 코드가 자동 컴파일됩니다.\n");

        foreach (var genFile in generatedCppFiles)
        {
            masterContent.AppendLine($"#include \"{genFile}\"");
        }

        File.WriteAllText(masterCppPath, masterContent.ToString());
        Console.WriteLine($"\n[UHT] 🌟 마스터 파일 생성 완료: {masterCppPath}");

        Console.WriteLine($"\n[UHT] 🎉 완료! 총 {parsedCount}개의 클래스를 파싱하여 생성했습니다.");
    }
}