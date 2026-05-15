using System;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Collections.Generic; // List를 사용하기 위해 추가

class CustomParser
{
    static string RemoveComments(string code)
    {
        StringBuilder result = new StringBuilder(code.Length);

        bool inLineComment = false;
        bool inBlockComment = false;
        bool inString = false;
        bool inChar = false;
        bool escape = false;

        for (int i = 0; i < code.Length; i++)
        {
            char c = code[i];
            char next = i + 1 < code.Length ? code[i + 1] : '\0';

            if (inLineComment)
            {
                if (c == '\r' || c == '\n')
                {
                    inLineComment = false;
                    result.Append(c);
                }
                continue;
            }

            if (inBlockComment)
            {
                if (c == '*' && next == '/')
                {
                    inBlockComment = false;
                    i++;
                }
                else if (c == '\r' || c == '\n')
                    result.Append(c);
                continue;
            }

            if (inString)
            {
                result.Append(c);

                if (escape)
                    escape = false;
                else if (c == '\\')
                    escape = true;
                else if (c == '"')
                    inString = false;
                continue;
            }

            if (inChar)
            {
                result.Append(c);

                if (escape)
                    escape = false;
                else if (c == '\\')
                    escape = true;
                else if (c == '\'')
                    inChar = false;
                continue;
            }

            if (c == '/' && next == '/')
            {
                inLineComment = true;
                i++;
                continue;
            }

            if (c == '/' && next == '*')
            {
                inBlockComment = true;
                i++;
                continue;
            }

            if (c == '"')
                inString = true;
            else if (c == '\'')
                inChar = true;

            result.Append(c);
        }

        return result.ToString();
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

        static readonly Regex ClassRegex = new Regex(
            @"UCLASS\s*\([^)]*\)\s*class\s+(?:[A-Z0-9_]+_API\s+)?(?<name>\w+)\s*:\s*public\s+(?<parent>\w+)",
            RegexOptions.Singleline);

        static readonly Regex PropertyRegex = new Regex(
            @"UPROPERTY\s*\((?<options>.*?)\)\s+(?<type>[A-Za-z_]\w*(?:::\w+)*(?:\s*<[^;{}()]+>)?\s*[*&]?)\s+(?<name>\w+)\s*(?:=.*?)?;",
            RegexOptions.Singleline);



        foreach (var file in headerFiles)
        {
            MatchCollection classMatches = ClassRegex.Matches(parseCode);
            if (classMatches.Count == 0)
            {
                continue;
            }

            string fileNameOnly = Path.GetFileNameWithoutExtension(file);
            string generatedHeaderPath = Path.Combine(outputDir, $"{fileNameOnly}.generated.h");
            string generatedCppPath = Path.Combine(outputDir, $"{fileNameOnly}.gen.cpp");

            StringBuilder headerContent = new StringBuilder();
            StringBuilder cppContent = new StringBuilder();

            headerContent.AppendLine("// [UHT 자동 생성 헤더 파일 - 직접 수정하지 마세요!]");
            headerContent.AppendLine("#pragma once");
            headerContent.AppendLine();

            cppContent.AppendLine("// [UHT 자동 생성 소스 파일 - 직접 수정하지 마세요!]");
            cppContent.AppendLine($"#include \"{includePath}\"");
            cppContent.AppendLine("#include \"Core/ReflectionDatabase.h\"");
            cppContent.AppendLine();

            for (int i = 0; i < classMatches.Count; i++)
            {
                Match classMatch = classMatches[i];
                string className = classMatch.Groups["name"].Value;
                string parentName = classMatch.Groups["parent"].Value;

                int classStart = classMatch.Index;
                int classEnd = (i + 1 < classMatches.Count)
                    ? classMatches[i + 1].Index
                    : parseCode.Length;

                string classBlock = parseCode.Substring(classStart, classEnd - classStart);

                parsedCount++;

                headerContent.AppendLine($"extern void Register_{className}();");
                headerContent.AppendLine();
                headerContent.AppendLine($"#define GENERATED_BODY_{className}() \\");
                headerContent.AppendLine("public: \\");
                headerContent.AppendLine($"    friend void Register_{className}(); \\");
                headerContent.AppendLine("    inline static FClassInfo StaticClassInfo; \\");
                headerContent.AppendLine("    virtual FClassInfo* GetStaticClass() override { return &StaticClassInfo; }");
                headerContent.AppendLine();

                cppContent.AppendLine($"void Register_{className}() {{");
                cppContent.AppendLine($"    FClassInfo& info = {className}::StaticClassInfo;");
                cppContent.AppendLine($"    info.ParentClassName = \"{parentName}\";");
                cppContent.AppendLine($"    info.ParentClass = ReflectionDatabase::GetClass(\"{parentName}\");");
                cppContent.AppendLine($"    info.ClassName = \"{className}\";");

                MatchCollection properties = PropertyRegex.Matches(classBlock);
                foreach (Match prop in properties)
                {
                    string options = prop.Groups["options"].Value;
                    string type = prop.Groups["type"].Value.Trim();
                    string name = prop.Groups["name"].Value;

                    bool bIsEditAnywhere = options.Contains("EditAnywhere");
                    string boolStr = bIsEditAnywhere ? "true" : "false";

                    cppContent.AppendLine(
                        $"    info.Properties.push_back({{ \"{name}\", \"{type}\", offsetof({className}, {name}), {boolStr} }});");

                    if (type.Contains("*"))
                    {
                        cppContent.AppendLine(
                            $"    info.GcPointerOffsets.push_back(offsetof({className}, {name}));");
                    }
                }

                cppContent.AppendLine($"    ReflectionDatabase::AddClass(\"{className}\", &info);");
                cppContent.AppendLine("}");
                cppContent.AppendLine();

                cppContent.AppendLine($"struct FAutoRegister_{className} {{");
                cppContent.AppendLine($"    FAutoRegister_{className}() {{ Register_{className}(); }}");
                cppContent.AppendLine("};");
                cppContent.AppendLine($"static FAutoRegister_{className} AutoRegister_{className}_Instance;");
                cppContent.AppendLine();
            }

            File.WriteAllText(generatedHeaderPath, headerContent.ToString());
            File.WriteAllText(generatedCppPath, cppContent.ToString());
            generatedCppFiles.Add($"{fileNameOnly}.gen.cpp");

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