using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;



class CustomParser
{
    static readonly Regex ClassRegex = new Regex(
        @"UCLASS\s*\([^)]*\)\s*class\s+(?:[A-Z0-9_]+_API\s+)?(?<name>\w+)\s*:\s*public\s+(?<parent>\w+)",
        RegexOptions.Singleline);

    static readonly Regex PropertyRegex = new Regex(
        @"UPROPERTY\s*\((?<options>[^)]*)\)\s+(?<type>[A-Za-z_]\w*(?:::\w+)*(?:\s*<[^;{}()]+>)?\s*[*&]?)\s+(?<name>\w+)\s*(?:=\s*[^;]*|\{[^;]*\})?\s*;",
        RegexOptions.Singleline);

    static readonly Regex StructRegex = new Regex(
        @"USTRUCT\s*\([^)]*\)\s*struct\s+(?:[A-Z0-9_]+_API\s+)?(?<name>\w+)(?:\s*:\s*(?:public|protected|private)?\s+(?<parent>\w+))?",
        RegexOptions.Singleline);

    static readonly Regex EnumRegex = new Regex(
        @"UENUM\s*\([^)]*\)\s*enum\s+class\s+(?:[A-Z0-9_]+_API\s+)?(?<name>\w+)",
        RegexOptions.Singleline);

    #region Helper Methods

    static List<string> ParseEnumValueNames(string enumBody)
    {
        List<string> values = new List<string>();

        int start = 0;
        int angleDepth = 0;
        int parenDepth = 0;
        int braceDepth = 0;

        for (int i = 0; i <= enumBody.Length; i++)
        {
            bool atEnd = i == enumBody.Length;
            char c = atEnd ? ',' : enumBody[i];

            if (!atEnd)
            {
                if (c == '<') angleDepth++;
                else if (c == '>') angleDepth--;
                else if (c == '(') parenDepth++;
                else if (c == ')') parenDepth--;
                else if (c == '{') braceDepth++;
                else if (c == '}') braceDepth--;
            }

            if ((atEnd || c == ',') && angleDepth == 0 && parenDepth == 0 && braceDepth == 0)
            {
                string item = enumBody.Substring(start, i - start).Trim();
                start = i + 1;

                if (item.Length == 0)
                    continue;

                int eq = item.IndexOf('=');
                if (eq >= 0)
                    item = item.Substring(0, eq).Trim();

                Match nameMatch = Regex.Match(item, @"^(?<name>[A-Za-z_]\w*)$");
                if (nameMatch.Success)
                    values.Add(nameMatch.Groups["name"].Value);
            }
        }

        return values;
    }

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
                {
                    result.Append(c);
                }
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

    static string EscapeForCppString(string value)
    {
        return value.Replace("\\", "\\\\").Replace("\"", "\\\"");
    }

    static string GetSourceRelativeInclude(string sourceDir, string file)
    {
        string prefix = sourceDir.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) + Path.DirectorySeparatorChar;
        if (file.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
            return file.Substring(prefix.Length).Replace('\\', '/');
        return Path.GetFileName(file);
    }
    static bool TryExtractBraceBlock(string code, int searchStart, out int openBraceIndex, out int closeBraceIndex, out string body)
    {
        openBraceIndex = -1;
        closeBraceIndex = -1;
        body = "";

        openBraceIndex = code.IndexOf('{', searchStart);
        if (openBraceIndex < 0)
            return false;

        int depth = 0;
        bool inString = false;
        bool inChar = false;
        bool escape = false;

        for (int i = openBraceIndex; i < code.Length; i++)
        {
            char c = code[i];

            if (inString)
            {
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
                if (escape)
                    escape = false;
                else if (c == '\\')
                    escape = true;
                else if (c == '\'')
                    inChar = false;

                continue;
            }

            if (c == '"')
            {
                inString = true;
                continue;
            }

            if (c == '\'')
            {
                inChar = true;
                continue;
            }

            if (c == '{')
            {
                depth++;
            }
            else if (c == '}')
            {
                depth--;

                if (depth == 0)
                {
                    closeBraceIndex = i;
                    body = code.Substring(openBraceIndex + 1, closeBraceIndex - openBraceIndex - 1);
                    return true;
                }
            }
        }

        return false;
    }

    #endregion

    static void Main(string[] args)
    {
        Console.OutputEncoding = Encoding.UTF8;

        string currentDir = args.Length > 0 ? args[0] : Directory.GetCurrentDirectory();
        currentDir = Path.GetFullPath(currentDir);

        Console.WriteLine($"\n[UHT] Parser cwd: {currentDir}");

        string projectRootDir = currentDir;
        while (!Directory.Exists(Path.Combine(projectRootDir, "Source")))
        {
            DirectoryInfo parent = Directory.GetParent(projectRootDir);
            if (parent == null)
            {
                Console.WriteLine("[UHT][Error] Could not find Source directory.");
                return;
            }
            projectRootDir = parent.FullName;
        }

        string sourceDir = Path.Combine(projectRootDir, "Source");
        string outputDir = Path.Combine(projectRootDir, "Intermediate", "Generated");
        Directory.CreateDirectory(outputDir);

        Console.WriteLine($"[UHT] Project root: {projectRootDir}");
        Console.WriteLine($"[UHT] Source: {sourceDir}");
        Console.WriteLine($"[UHT] Output: {outputDir}");

        var headerFiles = Directory.GetFiles(sourceDir, "*.h", SearchOption.AllDirectories);
        int parsedCount = 0;
        List<string> generatedCppFiles = new List<string>();

        foreach (var file in headerFiles)
        {
            if (file.EndsWith(".generated.h", StringComparison.OrdinalIgnoreCase))
                continue;

            bool bHasReflectionData = false;

            string cppCode = File.ReadAllText(file);
            string parseCode = RemoveComments(cppCode);

            MatchCollection classMatches = ClassRegex.Matches(parseCode);
            MatchCollection structMatches = StructRegex.Matches(parseCode);
            MatchCollection enumMatches = EnumRegex.Matches(parseCode);

            if (classMatches.Count == 0 && structMatches.Count == 0 && enumMatches.Count == 0)
                continue;

            string fileNameOnly = Path.GetFileNameWithoutExtension(file);
            string generatedHeaderPath = Path.Combine(outputDir, $"{fileNameOnly}.generated.h");
            string generatedCppPath = Path.Combine(outputDir, $"{fileNameOnly}.gen.cpp");
            string includePath = GetSourceRelativeInclude(sourceDir, file);

            StringBuilder headerContent = new StringBuilder();
            headerContent.AppendLine("// [UHT generated header - do not edit]");
            headerContent.AppendLine("#pragma once");
            headerContent.AppendLine();

            StringBuilder cppContent = new StringBuilder();
            cppContent.AppendLine("// [UHT generated source - do not edit]");
            cppContent.AppendLine($"#include \"{includePath}\"");
            cppContent.AppendLine("#include \"Core/ReflectionDatabase.h\"");
            cppContent.AppendLine();

            // ---------------------------------------------------------
            // 1. USTRUCT 파싱 로직 (새로 추가됨!)
            // ---------------------------------------------------------
            foreach (Match match in structMatches)
            {
                bHasReflectionData = true;

                // 이름과 부모 추출 (부모가 없으면 빈 문자열)
                string structName = match.Groups["name"].Value;
                string parentName = match.Groups["parent"].Success ? match.Groups["parent"].Value : "";

                Console.WriteLine($"\n [Parsing Success] Find Structure: {structName} (FineName: {fileNameOnly}.h)");

                // 구조체용 GENERATED_BODY_ClassName (구조체는 virtual 함수가 필요 없음)
                headerContent.AppendLine($@"extern void Register_{structName}();
#undef GENERATED_BODY_{structName} // 매크로 충돌 방지를 위해 이름 뒤에 구조체명 붙임
#define GENERATED_BODY_{structName}() \
public: \
    friend void Register_{structName}(); \
    inline static FStructInfo StaticStructInfo;
");

                cppContent.AppendLine($@"void Register_{structName}() {{
    FStructInfo& info = {structName}::StaticStructInfo;

    info.StructName = ""{structName}"";
    info.Size = sizeof({structName});");
                cppContent.AppendLine("    info.Properties.clear();");
                cppContent.AppendLine("    info.GcPointerOffsets.clear();");

                cppContent.AppendLine($"    info.ParentStructName = \"{EscapeForCppString(parentName)}\";");
                //if (!string.IsNullOrEmpty(parentName))
                //{
                //    cppContent.AppendLine($"    info.ParentStruct = ReflectionDatabase::GetStruct(\"{EscapeForCppString(parentName)}\");");
                //}

                // 구조체 내부 변수들(UPROPERTY) 찾기
                // (완벽한 파서를 원하신다면 괄호 { } 짝맞추기 로직이 추가되어야 합니다.)

                // 
                if (TryExtractBraceBlock(parseCode, match.Index, out _, out _, out string structBody))
                {
                    MatchCollection props = PropertyRegex.Matches(structBody);

                    foreach (Match prop in props)
                    {
                        string options = prop.Groups["options"].Value;
                        string type = prop.Groups["type"].Value.Trim();
                        string name = prop.Groups["name"].Value;

                        bool bIsEditAnywhere = options.Contains("EditAnywhere");
                        string boolStr = bIsEditAnywhere ? "true" : "false";

                        cppContent.AppendLine(
                            $"    info.Properties.push_back({{ \"{EscapeForCppString(name)}\", \"{EscapeForCppString(type)}\", offsetof({structName}, {name}), {boolStr} }});");

                        if (type.Contains("*"))
                        {
                            cppContent.AppendLine(
                                $"    info.GcPointerOffsets.push_back(offsetof({structName}, {name}));");
                        }
                    }
                }


                cppContent.AppendLine($@"    ReflectionDatabase::AddStruct(""{structName}"", &info);
}}
struct FAutoRegister_{structName} {{ FAutoRegister_{structName}() {{ Register_{structName}(); }} }};
static FAutoRegister_{structName} AutoRegister_{structName}_Instance;
");
            }

            // ---------------------------------------------------------
            // 2. UENUM 파싱 로직 
            // ---------------------------------------------------------

            foreach (Match match in enumMatches)
            {
                bHasReflectionData = true;

                string enumName = match.Groups["name"].Value;

                cppContent.AppendLine($"static FEnumInfo StaticEnumInfo_{enumName};");

                cppContent.AppendLine($@"void Register_{enumName}() {{
    FEnumInfo& info = StaticEnumInfo_{enumName};
    info.EnumName = ""{EscapeForCppString(enumName)}"";
    info.Values.clear();
");

                if (TryExtractBraceBlock(parseCode, match.Index, out _, out _, out string enumBody))
                {
                    foreach (string valueName in ParseEnumValueNames(enumBody))
                    {
                        cppContent.AppendLine(
                            $"    info.Values.push_back({{ \"{EscapeForCppString(valueName)}\", static_cast<int64>({enumName}::{valueName}) }});");
                        cppContent.AppendLine(
    $"    info.CachedNames.push_back(ReflectionUtils::GetStablePropertyName(FName(\"{EscapeForCppString(valueName)}\")));");
                    }
                }

                cppContent.AppendLine($@"    ReflectionDatabase::AddEnum(""{EscapeForCppString(enumName)}"", &info);
}}

struct FAutoRegister_{enumName}
{{
    FAutoRegister_{enumName}() {{ Register_{enumName}(); }}
}};
static FAutoRegister_{enumName} AutoRegister_{enumName}_Instance;
");
            }

            // ---------------------------------------------------------
            // 3. UCLASS 파싱 로직 (기존과 거의 동일, 단일 매치 -> 다중 매치로 변경)
            // ---------------------------------------------------------
            for (int i = 0; i < classMatches.Count; i++)
            {
                bHasReflectionData = true;

                Match classMatch = classMatches[i];
                string className = classMatch.Groups["name"].Value;
                string parentName = classMatch.Groups["parent"].Value;

                //int classStart = classMatch.Index;
                //int classEnd = (i + 1 < classMatches.Count)
                //    ? classMatches[i + 1].Index
                //    : parseCode.Length;
                //string classBlock = parseCode.Substring(classStart, classEnd - classStart);

                if (!TryExtractBraceBlock(parseCode, classMatch.Index, out _, out _, out string classBlock))
                    continue;


                parsedCount++;
                Console.WriteLine($"[UHT] Class: {className} : {parentName} ({includePath})");

                headerContent.AppendLine($"extern void Register_{className}();");
                headerContent.AppendLine();
                headerContent.AppendLine($"#define GENERATED_BODY_{className}() \\");
                headerContent.AppendLine("public: \\");
                headerContent.AppendLine($"    friend void Register_{className}(); \\");
                headerContent.AppendLine("    inline static FClassInfo StaticClassInfo; \\");
                headerContent.AppendLine("    virtual FClassInfo* GetStaticClass() override { return &StaticClassInfo; }");
                headerContent.AppendLine();

                cppContent.AppendLine($"void Register_{className}()");
                cppContent.AppendLine("{");
                cppContent.AppendLine($"    FClassInfo& info = {className}::StaticClassInfo;");
                cppContent.AppendLine("    info.Properties.clear();");
                cppContent.AppendLine("    info.GcPointerOffsets.clear();");
                cppContent.AppendLine($"    info.ClassName = \"{EscapeForCppString(className)}\";");
                cppContent.AppendLine($"    info.ParentClassName = \"{EscapeForCppString(parentName)}\";");
                //cppContent.AppendLine($"    info.ParentClass = ReflectionDatabase::GetClass(\"{EscapeForCppString(parentName)}\");");

                MatchCollection properties = PropertyRegex.Matches(classBlock);
                foreach (Match prop in properties)
                {
                    string options = prop.Groups["options"].Value;
                    string type = prop.Groups["type"].Value.Trim();
                    string name = prop.Groups["name"].Value;
                    bool bIsEditAnywhere = options.Contains("EditAnywhere");
                    string boolStr = bIsEditAnywhere ? "true" : "false";

                    Console.WriteLine($"   property: {type} {name} ({options})");
                    cppContent.AppendLine($"    info.Properties.push_back({{ \"{EscapeForCppString(name)}\", \"{EscapeForCppString(type)}\", offsetof({className}, {name}), {boolStr} }});");

                    if (type.Contains("*"))
                        cppContent.AppendLine($"    info.GcPointerOffsets.push_back(offsetof({className}, {name}));");
                }

                cppContent.AppendLine($"    ReflectionDatabase::AddClass(\"{EscapeForCppString(className)}\", &info);");
                cppContent.AppendLine("}");
                cppContent.AppendLine();
                cppContent.AppendLine($"struct FAutoRegister_{className}");
                cppContent.AppendLine("{");
                cppContent.AppendLine($"    FAutoRegister_{className}() {{ Register_{className}(); }}");
                cppContent.AppendLine("};");
                cppContent.AppendLine($"static FAutoRegister_{className} AutoRegister_{className}_Instance;");
                cppContent.AppendLine();
            }
            if (bHasReflectionData)
            {
                File.WriteAllText(generatedHeaderPath, headerContent.ToString());
                File.WriteAllText(generatedCppPath, cppContent.ToString());
                generatedCppFiles.Add($"{fileNameOnly}.gen.cpp");
            }
        }

        string masterCppPath = Path.Combine(outputDir, "JSEngine.Reflection.gen.cpp");
        StringBuilder masterContent = new StringBuilder();
        masterContent.AppendLine("// [UHT generated master source - do not edit]");
        masterContent.AppendLine();

        foreach (var genFile in generatedCppFiles)
            masterContent.AppendLine($"#include \"{genFile}\"");

        File.WriteAllText(masterCppPath, masterContent.ToString());
        Console.WriteLine($"\n[UHT] Generated master: {masterCppPath}");
        Console.WriteLine($"[UHT] Done. Parsed {parsedCount} classes.");
    }
}
